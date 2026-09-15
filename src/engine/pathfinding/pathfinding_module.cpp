#include "pathfinding_module.hpp"

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

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
// Path cache: memoize solved paths by (start node, goal node). In a small world many agents share
// the same start/goal tiles, so this turns most re-path requests into a cheap lookup. The graph is
// static within a scene, so cached paths stay valid until the nav graph is swapped; the
// PathCacheInvalidate observer clears the cache when NavEnvironment is (re)set. Guarded by a
// shared_mutex because PathRequestSystem is multi-threaded (concurrent reads on hits, exclusive
// writes on misses).
std::unordered_map<std::uint64_t, NavPath> g_path_cache;
std::shared_mutex g_path_cache_mutex;

// Guards unbounded growth: (start, goal) pairs are bounded by tiles² but can still be large, so drop
// the cache if it grows past this before an invalidation. Cheap and correct (paths get recomputed).
constexpr size_t kMaxPathCacheEntries = 200000;

std::uint64_t PathCacheKey(const graph::NodeId start, const graph::NodeId goal) {
	return (static_cast<std::uint64_t>(start) << 32) | static_cast<std::uint64_t>(goal);
}
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
		const std::unique_lock lock(g_path_cache_mutex);
		g_path_cache.clear();
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
	// Writes PathAgent and disables the PathRequest (consumed). Disabled requests are excluded from the
	// query, so only agents that actually need a path are processed. Results are memoized in the path
	// cache keyed by (start, goal) node so repeated identical requests skip the search.
	std::ignore = world.system<const PathRequest, const spatial::WorldTransform>("PathRequestSystem")
					  .kind(flecs::OnUpdate)
					  .multi_threaded(true)
					  .each([](const flecs::iter& it,
							   const size_t i,
							   const PathRequest& request,
							   const spatial::WorldTransform& wt) {
						  const auto& nav_env = it.world().get<NavEnvironment>();
						  if (!nav_env.IsReady()) return;

						  const auto& pathfinder_registry = it.world().get<PathfinderRegistry>();
						  const std::string& algo =
							  request.algorithm.empty() ? pathfinder_registry.default_algorithm : request.algorithm;
						  const NavAlgorithmFn* algo_fn = pathfinder_registry.Get(algo);
						  if (!algo_fn) return;

						  const graph::NodeId start = nav_env.graph->Locate(wt.position);
						  const graph::NodeId goal_node = nav_env.graph->Locate(request.goal);

						  const flecs::entity e = it.entity(i);
						  const float existing_radius = e.has<PathAgent>() ? e.get<PathAgent>().arrival_radius : 0.3f;

						  // Cache lookup (shared lock) → search on miss (no lock) → insert (exclusive lock).
						  const std::uint64_t key = PathCacheKey(start, goal_node);
						  NavPath path;
						  bool cache_hit = false;
						  {
							  const std::shared_lock lock(g_path_cache_mutex);
							  if (const auto found = g_path_cache.find(key); found != g_path_cache.end()) {
								  path = found->second;
								  cache_hit = true;
							  }
						  }
						  if (!cache_hit) {
							  path = (*algo_fn)(*nav_env.graph, start, goal_node);
							  const std::unique_lock lock(g_path_cache_mutex);
							  if (g_path_cache.size() >= kMaxPathCacheEntries) g_path_cache.clear();
							  g_path_cache.emplace(key, path);
						  }

						  PathAgent agent;
						  agent.waypoints = std::move(path.waypoints);
						  agent.current_waypoint = (agent.waypoints.size() > 1) ? 1 : 0;
						  agent.arrival_radius = existing_radius;
						  agent.reached_goal = !path.found || agent.waypoints.empty();
						  e.set<PathAgent>(agent);
						  e.disable<PathRequest>();
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
				if (agent.reached_goal || agent.waypoints.empty()) return;
				if (agent.current_waypoint >= agent.waypoints.size()) {
					agent.reached_goal = true;
					follow.desired_velocity = glm::vec3{0.0f};
					return;
				}

				const glm::vec3& target = agent.waypoints[agent.current_waypoint];
				const glm::vec3 to_target = target - wt.position;
				const float dist = glm::length(to_target);

				// Advance to next waypoint if within arrival radius
				if (dist <= agent.arrival_radius) {
					++agent.current_waypoint;
					if (agent.current_waypoint >= agent.waypoints.size()) {
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
				case PathFollowMode::Direct: transform.position += dir * follow.speed * dt; break;
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
						if (agent.waypoints.size() < 2) continue;

						// Path segments
						constexpr core::Rgba kLineColor{.r = 255, .g = 200, .b = 50, .a = 200};
						for (size_t idx = 1; idx < agent.waypoints.size(); ++idx) {
							platform->DrawLine(
								projector.Project(agent.waypoints[idx - 1]),
								projector.Project(agent.waypoints[idx]),
								2.0f,
								kLineColor
							);
						}

						// Nodes: start = green, goal = red, intermediate = yellow
						for (size_t idx = 0; idx < agent.waypoints.size(); ++idx) {
							constexpr float kNodeRadius = 4.0f;
							core::Rgba color;
							if (idx == 0) {
								color = {.r = 50, .g = 220, .b = 80, .a = 220};
							}
							else if (idx == agent.waypoints.size() - 1) {
								color = {.r = 220, .g = 60, .b = 60, .a = 220};
							}
							else {
								color = {.r = 255, .g = 220, .b = 50, .a = 180};
							}
							platform->DrawCircle(projector.Project(agent.waypoints[idx]), kNodeRadius, color);
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
}

} // namespace engine::pathfinding
