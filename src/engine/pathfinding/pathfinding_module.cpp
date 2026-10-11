#include "pathfinding_module.hpp"

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/profiling/profiling.hpp"

#include "engine/ai/ai.hpp"
#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/input/input.hpp"
#include "engine/level/level.hpp"
#include "engine/render/render.hpp"
#include "engine/scripting/scripting.hpp"
#include "engine/spatial/spatial.hpp"

#include "algorithms/astar_algorithm.hpp"
#include "algorithms/dijkstra_algorithm.hpp"
#include "engine/scene/scene_components.hpp"
#include "pathfinding_components.hpp"

namespace engine::pathfinding {

namespace {
// Cache value: a shared, immutable path plus whether a route was found. Because the waypoints are
// held by shared_ptr, PathCacheShard::Get and Put copy only a refcount under the shard lock instead
// of the whole vector. The previous in-lock vector copy serialised every thread that hit the same shard,
// which is why CacheLookup/CacheInsert zone times ballooned under concurrency.
//
// Eviction is safe for in-flight agents: evicting only drops the cache's shared_ptr ref; any agent
// still following that path keeps its own ref, so the (const, never-mutated) vector lives until the
// last owner — cache or agent — releases it.
struct CachedPath {
	SharedWaypoints waypoints;
	bool found{false};
};

// Fixed-capacity CLOCK cache shard. Thread-safe via an exclusive mutex.
//
// Replaces the previous LRU list+map design with a compact ring buffer (flat contiguous array of
// slots) plus an open-addressing hash table. This removes two performance cliffs at capacity:
//
//  1. LRU splice tax: the old design called list_.splice() on EVERY cache hit, randomly pointer-
//     chasing through non-contiguous list nodes. CLOCK eliminates this: a hit just sets a 1-bit
//     "referenced" flag in the slot in-place (a single cache-line write, no node movement).
//
//  2. Hash-map load-factor degradation: the old unordered_map was reserved at cap+1 entries with
//     default max_load_factor 1.0, so at full capacity it probed long collision chains on every
//     erase+insert during eviction. The map is now reserved at cap*2 / cap = 0.5 load factor
//     via max_load_factor(0.5), keeping hash ops O(1) in practice even at capacity.
//
// CLOCK eviction: the "hand" index advances through the ring, giving each slot a second chance
// (clears its referenced bit). The first unreferenced slot it reaches is evicted. All slots share
// a single flat vector, so both the sequential hand scan and the hash table lookups are cache-
// friendly. Total capacity: 64 × kShardEntries entries.
struct PathCacheShard {
	using Key = std::uint64_t;

	struct Slot {
		Key key{0};
		CachedPath path;
		bool referenced{false}; // CLOCK second-chance bit
	};

	explicit PathCacheShard(const size_t cap) : capacity_(cap), ring_(cap) {
		lookup_.max_load_factor(0.5f);
		lookup_.reserve(cap * 2 + 1);
	}

	// Returns the cached path (shared_ptr bump, no vector copy), or nullopt on miss.
	// Sets the CLOCK referenced bit in-place — no list splice, no node movement.
	[[nodiscard]] std::optional<CachedPath> Get(const Key key) {
		std::lock_guard lock(mutex_);
		const auto it = lookup_.find(key);
		if (it == lookup_.end()) return std::nullopt;
		Slot& slot = ring_[it->second];
		slot.referenced = true; // second-chance: won't be evicted on the next hand pass
		return slot.path;       // shared_ptr refcount bump — cheap
	}

	// Inserts or refreshes an entry. Returns true when a slot was evicted.
	// On a miss the CLOCK hand advances until it finds an unreferenced slot to reuse.
	bool Put(const Key key, CachedPath value) {
		std::lock_guard lock(mutex_);

		// Refresh an existing entry.
		if (const auto it = lookup_.find(key); it != lookup_.end()) {
			Slot& slot = ring_[it->second];
			slot.path = std::move(value);
			slot.referenced = true;
			return false;
		}

		// Find a victim slot via CLOCK: advance hand, skipping referenced entries.
		bool evicted = false;
		if (count_ < capacity_) {
			// Ring still has empty slots — use the next free index.
			hand_ = count_++;
		}
		else {
			// Ring full: advance the CLOCK hand past referenced entries (grant second chance).
			evicted = true;
			while (ring_[hand_].referenced) {
				ring_[hand_].referenced = false;
				hand_ = (hand_ + 1) % capacity_;
			}
			// Evict: remove old key from hash map.
			lookup_.erase(ring_[hand_].key);
		}

		Slot& slot = ring_[hand_];
		slot.key = key;
		slot.path = std::move(value);
		slot.referenced = true;
		lookup_[key] = hand_;
		hand_ = (hand_ + 1) % capacity_; // advance past the newly filled slot

		return evicted;
	}

	void Clear() {
		std::lock_guard lock(mutex_);
		for (auto& s : ring_) {
			s = {};
		}
		lookup_.clear();
		hand_ = 0;
		count_ = 0;
	}

	[[nodiscard]] size_t Size() const {
		std::lock_guard lock(mutex_);
		return count_;
	}

private:
	size_t capacity_;
	size_t hand_{0};  // CLOCK hand: next candidate for eviction
	size_t count_{0}; // entries filled so far (used to avoid eviction before the ring is full)
	std::vector<Slot> ring_;
	std::unordered_map<Key, size_t> lookup_; // key → ring index; reserved at 0.5 load factor
	mutable std::mutex mutex_;
};

// Sharded CLOCK cache: 64 independent shards keyed by MixKey(PathCacheKey) % kNumShards.
// Each shard carries its own mutex, so threads whose keys map to different shards never
// contend — a ~64× reduction vs a single global mutex under uniform key distribution.
// Total capacity: 64 × 6250 = 400 000 entries (~160 MB at ~400 B/path).
constexpr size_t kNumCacheShards = 64;
constexpr size_t kShardEntries = 400000 / kNumCacheShards;

// Initialise all shards at static-init time via an IIFE.
const auto g_cache_shards = []() {
	std::array<std::unique_ptr<PathCacheShard>, kNumCacheShards> arr;
	for (auto& p : arr) {
		p = std::make_unique<PathCacheShard>(kShardEntries);
	}
	return arr;
}();

// Mix all 64 bits of the key before selecting a shard. `key % kNumCacheShards` alone would select on
// the low 6 bits only — which, given PathCacheKey packs the goal node in the low 32 bits, means the
// shard was decided purely by `goal & 63`. Agents converging on shared goals then piled into a few
// shards, hitting the per-shard cap and evicting while total size plateaued well below capacity. A
// splitmix64 finalizer avalanches both start and goal into the low bits, spreading load evenly so the
// cache fills toward its full capacity and eviction/miss churn drops.
inline std::uint64_t MixKey(std::uint64_t k) {
	k ^= k >> 30;
	k *= 0xbf58476d1ce4e5b9ULL;
	k ^= k >> 27;
	k *= 0x94d049bb133111ebULL;
	k ^= k >> 31;
	return k;
}

inline PathCacheShard& CacheShard(const std::uint64_t key) { return *g_cache_shards[MixKey(key) % kNumCacheShards]; }

std::uint64_t PathCacheKey(const graph::NodeId start, const graph::NodeId goal) {
	return (static_cast<std::uint64_t>(start) << 32) | static_cast<std::uint64_t>(goal);
}

// Per-frame instrumentation counters. Reset by PathCacheStatsReset (OnLoad), plotted by
// PathCacheStatsPlot (PostFrame). Atomics because PathRequestSystem is multi-threaded.
std::atomic<int64_t> g_cache_hits{0};
std::atomic<int64_t> g_cache_misses{0};
std::atomic<int64_t> g_cache_evictions{0};
std::atomic<int64_t> g_path_requests{0}; // total requests processed (hits + misses)

// Counts distinct worker threads that execute PathRequestSystem per frame.
// Uses a generation counter so each thread can self-register once per frame without the
// reset system needing to reach into other threads' TLS storage.
std::atomic<int64_t> g_path_threads_active{0};
std::atomic<int64_t> g_frame_gen{0};
thread_local int64_t tl_path_frame_gen = -1;
} // namespace

PathfindingModule::PathfindingModule(const flecs::world& world) {
	// (1) Singletons
	if (!world.has<NavEnvironment>()) {
		world.emplace<NavEnvironment>();
	}
	if (!world.has<PathfinderRegistry>()) {
		world.set<PathfinderRegistry>({});
	}

	// Register built-in algorithms as type-erased NavAlgorithmFn entries
	auto& registry = world.get_mut<PathfinderRegistry>();
	registry.Add("astar", [](const INavGraph& g, const graph::NodeId s, const graph::NodeId gl) {
		return AStarAlgorithm{}.FindPath(g, s, gl);
	});
	registry.Add("dijkstra", [](const INavGraph& g, const graph::NodeId s, const graph::NodeId gl) {
		return DijkstraAlgorithm{}.FindPath(g, s, gl);
	});

	// (2) Reflection — pointer-to-member throughout: all registered components have fields
	// that are skipped (std::vector, size_t, enum, runtime state), so sequential tracking
	// would compute wrong offsets.
	world.component<PathFollowMode>()
		.constant("SteeringTarget", PathFollowMode::SteeringTarget)
		.constant("Kinematic", PathFollowMode::Kinematic)
		.constant("Direct", PathFollowMode::Direct);

	world.component<PathRequest>().member("goal", &PathRequest::goal).member("algorithm", &PathRequest::algorithm);
	// Toggle the request on/off instead of adding/removing it, avoiding per-cycle archetype moves.
	world.component<PathRequest>().add(flecs::CanToggle);

	// PathCacheInvalidate: clear the path cache whenever the nav graph is (re)set — a discrete event
	// (scene change), so an observer fits better than polling every frame. Callers that assign
	// NavEnvironment::graph via get_mut must emit the event with world.modified<NavEnvironment>().
	world.observer<NavEnvironment>("PathCacheInvalidate").event(flecs::OnSet).each([](NavEnvironment&) {
		for (const auto& s : g_cache_shards) {
			s->Clear();
		}
	});

	world.component<PathAgent>().member("arrival_radius", &PathAgent::arrival_radius);
	// waypoints / current_waypoint / reached_goal are runtime, not authored

	world.component<PathFollow>().member("speed", &PathFollow::speed);
	// mode (enum) and desired_velocity are authored/runtime fields skipped for simplicity

	world.component<PathAutoReplan>()
		.member("goal", &PathAutoReplan::goal)
		.member("algorithm", &PathAutoReplan::algorithm)
		.member("interval", &PathAutoReplan::interval);
	// timer is runtime state

	scripting::RegisterComponentForScripts(world, world.component<PathRequest>());
	scripting::RegisterComponentForScripts(world, world.component<PathAgent>());
	scripting::RegisterComponentForScripts(world, world.component<PathFollow>());
	scripting::RegisterComponentForScripts(world, world.component<PathAutoReplan>());

	// (3) Level loaders for JSON scene authoring
	level::RegisterComponentLoader(world, "path_agent", [](const flecs::entity e, const nlohmann::json& j) {
		PathAgent agent;
		agent.arrival_radius = j.value("arrival_radius", agent.arrival_radius);
		e.set<PathAgent>(agent);
	});

	level::RegisterComponentLoader(world, "path_follow", [](const flecs::entity e, const nlohmann::json& j) {
		PathFollow follow;
		follow.speed = j.value("speed", follow.speed);
		if (j.contains("mode")) {
			if (const auto mode_str = j.at("mode").get<std::string>(); mode_str == "kinematic") {
				follow.mode = PathFollowMode::Kinematic;
			}
			else if (mode_str == "direct") {
				follow.mode = PathFollowMode::Direct;
			}
		}
		e.set<PathFollow>(follow);
	});

	level::RegisterComponentLoader(world, "path_auto_replan", [](const flecs::entity e, const nlohmann::json& j) {
		PathAutoReplan replan;
		replan.goal = core::JVec3(j, "goal", replan.goal);
		replan.algorithm = j.value("algorithm", replan.algorithm);
		replan.interval = j.value("interval", replan.interval);
		e.set<PathAutoReplan>(replan);
	});

	// (4) Systems

	// PathAutoReplanSystem: periodically re-issue a PathRequest on entities with PathAutoReplan.
	// Runs before PathRequestSystem so the request is ready in the same frame.
	std::ignore = world.system<PathAutoReplan>("PathAutoReplanSystem")
					  .kind(flecs::OnUpdate)
					  .each([](const flecs::iter& it, const size_t i, PathAutoReplan& replan) {
						  replan.timer += it.delta_time();
						  if (replan.timer < replan.interval) return;
						  replan.timer = 0.0f;
						  const auto e = it.entity(i);
						  e.set<PathRequest>({.goal = replan.goal, .algorithm = replan.algorithm});
						  e.enable<PathRequest>();
					  })
					  .add<scene::GameScene>()
					  .add<ecs::Pausable>();

	// PathRequestSystem: compute a path for each entity carrying an ENABLED PathRequest + WorldTransform.
	// Registered with CanToggle: the system solves enabled requests and disables them (consumed), so
	// disabled requests are excluded from the query and only agents that actually need a path are
	// processed. Results are memoized in the path cache keyed by (start, goal) node so repeated identical
	// requests skip the search.
	// Requires PathAgent to be pre-added to the entity (spawn with PathAgent{}); writes to it directly
	// as a query field so there is no deferred e.set() structural command that would cause a main-thread
	// sync point under Flecs's multi-threaded pipeline.
	std::ignore = world.system<const PathRequest, const spatial::WorldTransform, PathAgent>("PathRequestSystem")
					  .kind(flecs::OnUpdate)
					  .multi_threaded(true)
					  .each([](const flecs::iter& it,
							   const size_t i,
							   const PathRequest& request,
							   const spatial::WorldTransform& wt,
							   PathAgent& agent) {
						  // Count distinct worker threads that perform real path work this frame using a
						  // generation stamp that all threads can observe — the main thread bumps the
						  // generation in OnLoad; each worker registers itself once when it sees the new gen.
						  const int64_t cur_gen = g_frame_gen.load(std::memory_order_relaxed);
						  if (tl_path_frame_gen != cur_gen) {
							  tl_path_frame_gen = cur_gen;
							  g_path_threads_active.fetch_add(1, std::memory_order_relaxed);
						  }

						  const auto& nav_env = it.world().get<NavEnvironment>();
						  if (!nav_env.IsReady()) return;

						  const auto& pathfinder_registry = it.world().get<PathfinderRegistry>();
						  const std::string& algo =
							  request.algorithm.empty() ? pathfinder_registry.default_algorithm : request.algorithm;
						  const NavAlgorithmFn* algo_fn = pathfinder_registry.Get(algo);
						  if (!algo_fn) return;

						  const graph::NodeId start = nav_env.graph->Locate(wt.position);
						  const graph::NodeId goal_node = nav_env.graph->Locate(request.goal);

						  // Sharded LRU lookup: the key determines the shard, so threads with different
						  // (start, goal) pairs hitting different shards never contend. A* runs
						  // outside the lock; only the brief insert touches it.
						  const std::uint64_t key = PathCacheKey(start, goal_node);
						  auto& shard = CacheShard(key);

						  CachedPath resolved;
						  bool cache_hit = false;
						  {
							  profiling::Zone _zone{"PathRequest::CacheLookup"};
							  if (auto cached = shard.Get(key)) {
								  resolved = std::move(*cached);
								  cache_hit = true;
							  }
						  }

						  if (!cache_hit) {
							  NavPath path = (*algo_fn)(*nav_env.graph, start, goal_node); // A*::FindPath zone inside
							  resolved.found = path.found;
							  // One allocation shared by the cache and every agent on this route (net memory
							  // win vs a per-agent copy). Immutable, so concurrent readers need no lock.
							  resolved.waypoints =
								  std::make_shared<const std::vector<glm::vec3>>(std::move(path.waypoints));
							  {
								  profiling::Zone _zone{"PathRequest::CacheInsert"};
								  if (shard.Put(key, resolved)) {
									  g_cache_evictions.fetch_add(1, std::memory_order_relaxed);
								  }
							  }
						  }

						  g_path_requests.fetch_add(1, std::memory_order_relaxed);
						  if (cache_hit) {
							  g_cache_hits.fetch_add(1, std::memory_order_relaxed);
						  }
						  else {
							  g_cache_misses.fetch_add(1, std::memory_order_relaxed);
						  }

						  {
							  profiling::Zone _zone{"PathRequest::SetAgent"};
							  // Write directly into the pre-existing PathAgent field — no deferred
							  // e.set() command, no structural change, no main-thread sync point.
							  const size_t count = resolved.waypoints ? resolved.waypoints->size() : 0;
							  agent.waypoints = std::move(resolved.waypoints); // shares the allocation
							  agent.current_waypoint = (count > 1) ? 1 : 0;
							  // arrival_radius preserved as-is
							  agent.reached_goal = !resolved.found || count == 0;
							  it.entity(i).disable<PathRequest>();
						  }
					  })
					  .add<scene::GameScene>()
					  .add<ecs::Pausable>();

	// PathFollowSystem: advance the entity along its computed path and produce movement output.
	std::ignore =
		world.system<PathAgent, PathFollow, const spatial::WorldTransform, spatial::Transform>("PathFollowSystem")
			.kind(flecs::OnUpdate)
			.multi_threaded(true)
			.each([](const flecs::iter& it,
					 const size_t i,
					 PathAgent& agent,
					 PathFollow& follow,
					 const spatial::WorldTransform& wt,
					 spatial::Transform& transform) {
				if (agent.reached_goal || !agent.HasPath()) return;
				if (agent.current_waypoint >= agent.WaypointCount()) {
					agent.reached_goal = true;
					follow.desired_velocity = glm::vec3{0.0f};
					return;
				}

				const glm::vec3& target = agent.Waypoint(agent.current_waypoint);
				const glm::vec3 to_target = target - wt.position;
				const float dist = glm::length(to_target);

				// Advance to next waypoint if within arrival radius
				if (dist <= agent.arrival_radius) {
					++agent.current_waypoint;
					if (agent.current_waypoint >= agent.WaypointCount()) {
						agent.reached_goal = true;
						follow.desired_velocity = glm::vec3{0.0f};
					}
					return;
				}

				const glm::vec3 dir = to_target / dist;
				const float dt = it.delta_time();

				switch (follow.mode) {
				case PathFollowMode::SteeringTarget: it.entity(i).set<ai::SteeringTarget>({.position = target}); break;
				case PathFollowMode::Kinematic: follow.desired_velocity = dir * follow.speed; break;
				case PathFollowMode::Direct:
				{
					// Clamp the step to the remaining distance so a large frame delta (e.g. a spawn-frame
					// hitch) can't overshoot the waypoint and fling the agent across the map.
					const float step = std::min(follow.speed * dt, dist);
					transform.position += dir * step;
					break;
				}
				}
			})
			.add<scene::GameScene>()
			.add<ecs::Pausable>();

	// Build the camera query once and capture it, rather than constructing a query per agent per
	// frame (the previous shape). Converted to .run so the platform, camera and screen projector are
	// resolved once per frame instead of once per agent.
	const auto debug_camera_query = world.query_builder<const spatial::WorldTransform, const render::Camera>().build();

	const auto debug_draw_sys =
		world.system<const PathAgent>("PathDebugDrawSystem")
			.kind<ecs::OnUI>()
			.run([debug_camera_query](flecs::iter& it) {
				auto* platform = it.world().get<platform::PlatformRef>().ptr;
				const auto cam_e = debug_camera_query.first();
				if (platform == nullptr || !cam_e) {
					while (it.next()) {
						// Drain the iterator so the system completes cleanly when there is nothing to draw.
					}
					return;
				}

				const auto projector = render::ScreenProjector::FromCamera(
					cam_e,
					static_cast<float>(platform->Width()),
					static_cast<float>(platform->Height())
				);

				while (it.next()) {
					const auto agents = it.field<const PathAgent>(0);
					for (const auto i : it) {
						const PathAgent& agent = agents[i];
						if (agent.WaypointCount() < 2) continue;

						// Path segments
						constexpr core::Rgba kLineColor{.r = 255, .g = 200, .b = 50, .a = 200};
						for (size_t idx = 1; idx < agent.WaypointCount(); ++idx) {
							platform->DrawLine(
								projector.Project(agent.Waypoint(idx - 1)),
								projector.Project(agent.Waypoint(idx)),
								2.0f,
								kLineColor
							);
						}

						// Nodes: start = green, goal = red, intermediate = yellow
						for (size_t idx = 0; idx < agent.WaypointCount(); ++idx) {
							constexpr float kNodeRadius = 4.0f;
							core::Rgba color;
							if (idx == 0) {
								color = {.r = 50, .g = 220, .b = 80, .a = 220};
							}
							else if (idx == agent.WaypointCount() - 1) {
								color = {.r = 220, .g = 60, .b = 60, .a = 220};
							}
							else {
								color = {.r = 255, .g = 220, .b = 50, .a = 180};
							}
							platform->DrawCircle(projector.Project(agent.Waypoint(idx)), kNodeRadius, color);
						}
					}
				}
			})
			.add<scene::GameScene>();

	if (!debug_draw_sys.disable()) {
		spdlog::warn("[PathfindingModule] Failed to disable PathDebugDrawSystem initially");
	}

	std::ignore = world.system<const input::InputState>("PathDebugToggle")
					  .kind(flecs::OnUpdate)
					  .term_at<input::InputState>()
					  .singleton()
					  .each([debug_draw_sys](const flecs::iter&, size_t, const input::InputState& input) {
						  if (!input.keys[input::kDebugRenderKey].pressed) return;
						  if (debug_draw_sys.enabled()) {
							  if (!debug_draw_sys.disable()) {
								  spdlog::warn("[PathDebugToggle] Failed to disable");
							  }
						  }
						  else {
							  if (!debug_draw_sys.enable()) {
								  spdlog::warn("[PathDebugToggle] Failed to enable");
							  }
						  }
					  })
					  .add<scene::GameScene>();

	spdlog::info("[PathfindingModule] Registered pathfinding systems with Flecs");

	// Reset per-frame cache counters at the start of every frame, before PathRequestSystem runs.
	std::ignore = world.system("PathCacheStatsReset")
					  .kind(flecs::OnLoad)
					  .run([](flecs::iter&) {
						  g_cache_hits.store(0, std::memory_order_relaxed);
						  g_cache_misses.store(0, std::memory_order_relaxed);
						  g_cache_evictions.store(0, std::memory_order_relaxed);
						  g_path_requests.store(0, std::memory_order_relaxed);
						  g_path_threads_active.store(0, std::memory_order_relaxed);
						  g_frame_gen.fetch_add(1, std::memory_order_relaxed); // signal new frame to all TLS slots
					  })
					  .add<scene::GameScene>();

	// Emit per-frame cache stats. RegisterStatsEmitter owns the ProfileStats entity + plumbing.
	std::ignore =
		profiling::RegisterStatsEmitter(
			world,
			"PathCacheStatsPlot",
			[](const flecs::world& w, profiling::ProfileStats& stats) {
				stats.Plot("PathfindingModule", "path_cache.requests", g_path_requests.load(std::memory_order_relaxed));
				stats.Plot("PathfindingModule", "path_cache.hits", g_cache_hits.load(std::memory_order_relaxed));
				stats.Plot("PathfindingModule", "path_cache.misses", g_cache_misses.load(std::memory_order_relaxed));
				stats.Plot(
					"PathfindingModule",
					"path_cache.evictions",
					g_cache_evictions.load(std::memory_order_relaxed)
				);
				stats.Plot(
					"PathfindingModule",
					"pathrequest.threads",
					static_cast<int64_t>(g_path_threads_active.load(std::memory_order_relaxed))
				);
				int64_t total_size = 0;
				for (const auto& s : g_cache_shards) {
					total_size += static_cast<int64_t>(s->Size());
				}
				stats.Plot("PathfindingModule", "path_cache.size", total_size);
				stats.Plot(
					"PathfindingModule",
					"path_request.total_entities",
					static_cast<int64_t>(w.count<PathRequest>())
				);
			}
		).add<scene::GameScene>();
}

} // namespace engine::pathfinding
