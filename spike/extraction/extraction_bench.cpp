// Throwaway (wayfinder ticket 09): what extracting every draw each Frame, unculled, costs. Headless, no drawing.
//
//   extraction_bench --scene mixed --entities 5200 --threads 4 [--iters 300] [--cold] [--physics]
//
// Prints one tab-separated row per timed step: scene, entities, threads, flags, step, median us, p95 us, min us.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "engine/engine.hpp"
#include "engine/platform/render_batch.hpp"
#include "engine/render/render_queue.hpp"

namespace {

using engine::physics::CollisionShape;
using engine::physics::RigidBody;
using engine::platform::DrawMaterial;
using engine::platform::RenderBatchGroup;
using engine::platform::RenderCommand;
using engine::render::AlbedoMap;
using engine::render::CubePrimitive;
using engine::render::Frustum;
using engine::render::Material;
using engine::render::RenderWith;
using engine::render::ShaderMap;
using engine::render::Translucent;
using engine::spatial::Transform;
using engine::spatial::WorldTransform;
using Clock = std::chrono::steady_clock;

// ── The draw list ───────────────────────────────────────────────────────────────────────────────

// The plan's DrawItem {mesh, material, transform, flags, bounds, entity, part}.
struct DrawItem {
	glm::mat4 transform;
	glm::vec4 bounds; // world center, radius
	uint64_t entity;
	uint32_t part;
	uint32_t flags;
	int mesh;
	DrawMaterial material;
};

// A vector that never value-initializes and keeps its memory, so only the writes are timed.
struct ItemBuffer {
	std::unique_ptr<DrawItem[]> data;
	size_t size{0};
	size_t capacity{0};

	DrawItem* Grow(const size_t n) {
		if (size + n > capacity) {
			const size_t next = std::max(capacity * 2, size + n);
			auto bigger = std::make_unique_for_overwrite<DrawItem[]>(next);
			if (size > 0) std::memcpy(bigger.get(), data.get(), size * sizeof(DrawItem));
			data = std::move(bigger);
			capacity = next;
		}
		DrawItem* out = data.get() + size;
		size += n;
		return out;
	}
};

struct alignas(64) PaddedBuffer {
	ItemBuffer items;
	size_t tables{0};
};

// One matched table slice, gathered on the main thread so workers can split by entity count, not per table.
struct Span {
	const WorldTransform* wt;
	const CubePrimitive* cube;
	const Material* inline_mat; // null: `shared` applies to the whole slice
	const flecs::entity_t* entities;
	DrawMaterial shared;
	size_t count;
	size_t first; // index of its first item in the list
};

constexpr int kMaxStages = 64;
std::array<PaddedBuffer, kMaxStages> g_stage;
ItemBuffer g_joined;
std::vector<Span> g_spans;

float MaxAxisScale(const glm::mat4& m) {
	return glm::max(glm::length(glm::vec3(m[0])), glm::max(glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2]))));
}

DrawMaterial MaterialOf(const Material& m) {
	return {
		.tint = m.color,
		.wireframe = m.wireframe,
		.cast_shadow = m.cast_shadow,
		.unlit = m.unlit,
		.double_sided = m.double_sided,
	};
}

// Today's MakeDrawMaterial: three try_gets per call.
DrawMaterial MaterialOfEntity(const flecs::entity e) {
	if (!e.is_valid()) return {};
	const auto* mat = e.try_get<Material>();
	if (mat == nullptr) return {};
	const auto* albedo = e.try_get<AlbedoMap>();
	const auto* shader = e.try_get<ShaderMap>();
	DrawMaterial out = MaterialOf(*mat);
	out.texture = albedo != nullptr ? albedo->handle : -1;
	out.shader = shader != nullptr ? shader->handle : -1;
	return out;
}

void WriteItem(
	DrawItem& out,
	const WorldTransform& wt,
	const CubePrimitive& cube,
	const uint64_t entity,
	const DrawMaterial& material
) {
	out.transform = wt.matrix;
	out.transform[0] *= cube.size.x;
	out.transform[1] *= cube.size.y;
	out.transform[2] *= cube.size.z;
	out.bounds = glm::vec4(glm::vec3(out.transform[3]), 0.8660254F * MaxAxisScale(out.transform) * 1.1F);
	out.entity = entity;
	out.part = 0;
	out.flags = material.cast_shadow ? 1U : 0U;
	out.mesh = 0;
	out.material = material;
}

struct Hidden {};
struct OtherTag {};
struct Mover {};
struct TagEmpty {};
struct TagPerEntity {};
struct TagPerTable {};
struct TagPerTableHidden {};
struct TagHiddenUncached {};
struct TagHiddenSearch {};
struct TagSpanWork {};
struct TagRenderCubes {};
struct TagViews {};
struct SpanJob {
	int index{0};
};
struct ViewJob {
	int index{0};
};

// ── Views ───────────────────────────────────────────────────────────────────────────────────────

struct Instance {
	glm::mat4 transform;
	engine::core::Rgba tint;
};

struct View {
	Frustum frustum;
	std::vector<RenderCommand> commands;
	std::vector<uint32_t> order;
	std::vector<RenderBatchGroup> groups;
	std::vector<uint64_t> keyed; // the alternative: (key << 32 | item index), sorted
	std::vector<Instance> instances;
	size_t visible{0};
};
std::array<View, 6> g_views;

uint32_t PackKey(const DrawItem& d) {
	return (static_cast<uint32_t>(d.mesh + 1) << 20) | (static_cast<uint32_t>(d.material.texture + 1) << 10) |
		   (static_cast<uint32_t>(d.material.shader + 1) << 4) | (d.material.wireframe ? 8U : 0U) |
		   (d.material.cast_shadow ? 4U : 0U) | (d.material.unlit ? 2U : 0U) | (d.material.double_sided ? 1U : 0U);
}

void CullOnly(View& v, const ItemBuffer& list) {
	size_t n = 0;
	for (size_t i = 0; i < list.size; ++i) {
		const DrawItem& d = list.data[i];
		n += v.frustum.ContainsSphere(glm::vec3(d.bounds), d.bounds.w) ? 1 : 0;
	}
	v.visible = n;
}

// Through render_batch.cpp, as the ticket asks: copy the visible items to RenderCommands and group them.
void CullAndGroup(View& v, const ItemBuffer& list) {
	v.commands.clear();
	for (size_t i = 0; i < list.size; ++i) {
		const DrawItem& d = list.data[i];
		if (!v.frustum.ContainsSphere(glm::vec3(d.bounds), d.bounds.w)) continue;
		v.commands.push_back({.mesh = d.mesh, .model = -1, .transform = d.transform, .material = d.material});
	}
	v.visible = v.commands.size();
	v.groups = engine::platform::GroupRenderCommands(v.commands, v.order);
}

// The alternative: sort 8-byte (key, index) pairs and copy nothing.
void CullAndSortKeys(View& v, const ItemBuffer& list) {
	v.keyed.clear();
	for (size_t i = 0; i < list.size; ++i) {
		const DrawItem& d = list.data[i];
		if (!v.frustum.ContainsSphere(glm::vec3(d.bounds), d.bounds.w)) continue;
		v.keyed.push_back((static_cast<uint64_t>(PackKey(d)) << 32) | i);
	}
	std::ranges::sort(v.keyed);
	v.visible = v.keyed.size();
}

void WriteInstances(View& v, const ItemBuffer& list) {
	if (v.instances.size() < v.keyed.size()) v.instances.resize(v.keyed.size());
	for (size_t i = 0; i < v.keyed.size(); ++i) {
		const DrawItem& d = list.data[v.keyed[i] & 0xFFFFFFFFU];
		v.instances[i] = {.transform = d.transform, .tint = d.material.tint};
	}
}

void BuildViews() {
	const glm::vec3 eye{0.0F, 15.0F, 40.0F};
	const glm::mat4 look = glm::lookAt(eye, glm::vec3{0.0F, 0.0F, -10.0F}, glm::vec3{0, 1, 0});
	g_views[0].frustum.FromViewProj(glm::perspective(glm::radians(60.0F), 16.0F / 9.0F, 0.1F, 100.0F) * look);
	const std::array<float, 4> extents{10.0F, 25.0F, 60.0F, 150.0F};
	const glm::vec3 sun = glm::normalize(glm::vec3{-0.5F, -1.0F, -0.35F});
	for (size_t i = 0; i < extents.size(); ++i) {
		const float e = extents[i];
		const glm::vec3 center{0.0F, 0.0F, 40.0F - e};
		const glm::mat4 view = glm::lookAt(center - sun * 200.0F, center, glm::vec3{0, 0, 1});
		g_views[1 + i].frustum.FromViewProj(glm::ortho(-e, e, -e, e, 0.1F, 400.0F) * view);
	}
	const glm::mat4 spot = glm::lookAt(glm::vec3{20, 10, 0}, glm::vec3{20, 0, 0.1F}, glm::vec3{0, 0, 1});
	g_views[5].frustum.FromViewProj(glm::perspective(glm::radians(90.0F), 1.0F, 0.1F, 30.0F) * spot);
}

// ── RenderScene ─────────────────────────────────────────────────────────────────────────────────

struct Slot {
	glm::mat4 matrix;
	glm::mat4 prev;
	glm::vec4 bounds;
	uint64_t entity;
	uint32_t part;
	uint32_t last_seen;
};

struct Changes {
	std::vector<uint32_t> added;
	std::vector<std::pair<uint32_t, glm::vec4>> moved; // slot, old bounds
	std::vector<glm::vec4> removed;                    // old bounds

	void Clear() {
		added.clear();
		moved.clear();
		removed.clear();
	}
};

uint64_t HashKey(const uint64_t entity, const uint32_t part) {
	uint64_t h = entity * 0x9E3779B97F4A7C15ULL + part * 0xC2B2AE3D27D4EB4FULL;
	h ^= h >> 32;
	return h * 0xD6E8FEB86659FD93ULL;
}

// Open addressing over dense slots, linear probing, backward-shift deletion.
struct FlatIndex {
	std::vector<uint32_t> cells; // slot + 1; 0 is empty
	size_t mask{0};
	size_t used{0};

	void Reserve(const size_t n, const std::vector<Slot>& slots) {
		if (!cells.empty() && n * 2 <= cells.size()) return;
		size_t cap = 1024;
		while (cap < n * 2) cap *= 2;
		const std::vector<uint32_t> old = std::move(cells);
		cells.assign(cap, 0);
		mask = cap - 1;
		used = 0;
		for (const uint32_t c : old) {
			if (c != 0) Insert(c - 1, slots);
		}
	}
	[[nodiscard]] size_t Home(const Slot& s) const { return (HashKey(s.entity, s.part) >> 20) & mask; }
	int64_t Find(const uint64_t entity, const uint32_t part, const std::vector<Slot>& slots) const {
		for (size_t i = (HashKey(entity, part) >> 20) & mask;; i = (i + 1) & mask) {
			if (cells[i] == 0) return -1;
			const Slot& s = slots[cells[i] - 1];
			if (s.entity == entity && s.part == part) return cells[i] - 1;
		}
	}
	void Insert(const uint32_t slot, const std::vector<Slot>& slots) {
		size_t i = Home(slots[slot]);
		while (cells[i] != 0) i = (i + 1) & mask;
		cells[i] = slot + 1;
		++used;
	}
	void Erase(const uint32_t slot, const std::vector<Slot>& slots) {
		size_t i = Home(slots[slot]);
		while (cells[i] != slot + 1) i = (i + 1) & mask;
		size_t j = i;
		for (;;) {
			j = (j + 1) & mask;
			if (cells[j] == 0) break;
			const size_t home = Home(slots[cells[j] - 1]);
			if (((j - home) & mask) >= ((j - i) & mask)) {
				cells[i] = cells[j];
				i = j;
			}
		}
		cells[i] = 0;
		--used;
	}
};

struct KeyHash {
	size_t operator()(const std::pair<uint64_t, uint32_t>& k) const { return HashKey(k.first, k.second); }
};

// kStdMap: std::unordered_map. kFlat: open addressing. kHint: the slot this list position used last frame, then kFlat.
enum class Lookup { StdMap, Flat, Hint };

struct RenderScene {
	Lookup lookup;
	std::vector<Slot> slots;
	std::vector<uint32_t> free_slots;
	std::vector<uint32_t> hint; // per list position
	FlatIndex flat;
	std::unordered_map<std::pair<uint64_t, uint32_t>, uint32_t, KeyHash> map;
	Changes changes;
	uint32_t frame{0};

	explicit RenderScene(const Lookup l) : lookup(l) {}

	int64_t Find(const size_t position, const DrawItem& d) {
		if (lookup == Lookup::StdMap) {
			const auto it = map.find({d.entity, d.part});
			return it == map.end() ? int64_t{-1} : int64_t{it->second};
		}
		if (lookup == Lookup::Hint && position < hint.size()) {
			const uint32_t h = hint[position];
			if (slots[h].entity == d.entity && slots[h].part == d.part) return h;
		}
		return flat.Find(d.entity, d.part, slots);
	}

	void Compare(const ItemBuffer& list) {
		++frame;
		changes.Clear();
		flat.Reserve(list.size + 1024, slots);
		if (hint.size() < list.size) hint.resize(list.size, 0);
		for (size_t i = 0; i < list.size; ++i) {
			const DrawItem& d = list.data[i];
			int64_t found = slots.empty() ? -1 : Find(i, d);
			if (found < 0) {
				uint32_t s = 0;
				if (free_slots.empty()) {
					s = static_cast<uint32_t>(slots.size());
					slots.emplace_back();
				}
				else {
					s = free_slots.back();
					free_slots.pop_back();
				}
				slots[s] = {d.transform, d.transform, d.bounds, d.entity, d.part, frame};
				if (lookup == Lookup::StdMap) map.emplace(std::pair{d.entity, d.part}, s);
				else flat.Insert(s, slots);
				changes.added.push_back(s);
				hint[i] = s;
				continue;
			}
			Slot& slot = slots[static_cast<size_t>(found)];
			hint[i] = static_cast<uint32_t>(found);
			slot.last_seen = frame;
			if (std::memcmp(&slot.matrix, &d.transform, sizeof(glm::mat4)) != 0) {
				changes.moved.emplace_back(static_cast<uint32_t>(found), slot.bounds);
				slot.prev = slot.matrix;
				slot.matrix = d.transform;
				slot.bounds = d.bounds;
			}
		}
		// Anything not seen is gone.
		for (uint32_t s = 0; s < slots.size(); ++s) {
			if (slots[s].entity == 0 || slots[s].last_seen == frame) continue;
			changes.removed.push_back(slots[s].bounds);
			if (lookup == Lookup::StdMap) map.erase({slots[s].entity, slots[s].part});
			else flat.Erase(s, slots);
			slots[s].entity = 0;
			free_slots.push_back(s);
		}
	}
};

// ── Worlds ──────────────────────────────────────────────────────────────────────────────────────

struct Options {
	std::string scene{"mixed"};
	int entities{5200};
	int threads{0};
	int iters{300};
	bool cold{false};
	bool physics{false};
	int bodies{2000};
};

struct Builder {
	flecs::world& world;
	flecs::entity root;
	std::array<flecs::entity, 6> materials;
	int total;
	int side;
	int placed{0};

	Builder(flecs::world& w, const int n) :
			world(w), root(w.entity().set<Transform>({})), total(n),
			side(static_cast<int>(std::ceil(std::sqrt(static_cast<float>(n))))) {
		// Flag combinations, so the six materials make six batch keys.
		for (int i = 0; i < 6; ++i) {
			materials[i] = world.entity().child_of(root).set<Material>(
				{.cast_shadow = (i & 1) == 0, .unlit = (i & 2) != 0, .double_sided = (i & 4) != 0}
			);
		}
	}
	// A 2 m grid, centered on the origin.
	glm::vec3 Next() {
		const int i = placed++;
		const float half = static_cast<float>(side);
		return {static_cast<float>(i % side) * 2.0F - half, 0.5F, static_cast<float>(i / side) * 2.0F - half};
	}
	Transform At(const glm::vec3 p, const bool rotated) const {
		return {.position = p, .rotation = {0.0F, rotated ? 0.7F : 0.0F, 0.0F}};
	}
	void Flat(const int n, const bool inline_mat) {
		for (int i = 0; i < n; ++i) {
			auto e = world.entity().child_of(root).set<Transform>(At(Next(), (i & 1) != 0)).set<CubePrimitive>({});
			if (inline_mat) e.set<Material>({.color = {.r = static_cast<uint8_t>(i), .g = 90, .b = 90, .a = 255}});
			else e.add<RenderWith>(materials[i % 6]);
		}
	}
	// Moving parents with four drawn children. kSame: one table of four per parent. kDifferent: four tables of one.
	// kInherit: the parent holds RenderWith.
	enum class Mat { Same, Different, Inherit };
	void Groups(const int n, const Mat mode) {
		for (int g = 0; g < n / 4; ++g) {
			auto parent = world.entity().child_of(root).set<Transform>(At(Next(), true)).add<Mover>();
			if (mode == Mat::Inherit) parent.add<RenderWith>(materials[g % 6]);
			for (int c = 0; c < 4; ++c) {
				auto e = world.entity()
							 .child_of(parent)
							 .set<Transform>(At({static_cast<float>(c) * 0.5F, 0.0F, 0.0F}, false))
							 .set<CubePrimitive>({});
				if (mode == Mat::Same) e.add<RenderWith>(materials[g % 6]);
				if (mode == Mat::Different) e.add<RenderWith>(materials[c % 6]);
			}
			placed += 3;
		}
	}
	// Chains four deep, every link drawn, so each is alone in its table.
	void Chains(const int n) {
		for (int g = 0; g < n / 4; ++g) {
			flecs::entity parent = root;
			for (int d = 0; d < 4; ++d) {
				const glm::vec3 p = d == 0 ? Next() : glm::vec3{0.5F, 0.2F, 0.0F};
				auto e = world.entity()
							 .child_of(parent)
							 .set<Transform>(At(p, d == 0))
							 .set<CubePrimitive>({})
							 .add<RenderWith>(materials[g % 6]);
				if (d == 0) e.add<Mover>();
				parent = e;
			}
			placed += 3;
		}
	}
	bool Build(const std::string& scene) {
		if (scene == "flat_inline") Flat(total, true);
		else if (scene == "flat_shared") Flat(total, false);
		else if (scene == "groups_same") Groups(total, Mat::Same);
		else if (scene == "groups_diff") Groups(total, Mat::Different);
		else if (scene == "groups_inherit") Groups(total, Mat::Inherit);
		else if (scene == "chains") Chains(total);
		else if (scene == "mixed") {
			Flat(total * 4 / 10, false);
			Flat(total * 2 / 10, true);
			Groups(total - total * 4 / 10 - total * 2 / 10, Mat::Same);
		}
		else return false;
		return true;
	}
};

flecs::entity FindSystem(const flecs::world& world, const std::string& name) {
	flecs::entity found;
	world.query_builder<>().with(flecs::System).build().each([&](const flecs::entity e) {
		if (e.name() && name == e.name().c_str()) found = e;
	});
	if (!found) {
		std::fprintf(stderr, "no system named %s\n", name.c_str());
		std::exit(1);
	}
	return found;
}

template<typename Tag>
flecs::entity MakePipeline(const flecs::world& world) {
	return world.pipeline().with(flecs::System).template with<Tag>().build();
}

// One table slice, resolved per entity as RenderCubes does today.
void ExtractPerEntity(flecs::iter& it) {
	PaddedBuffer& out = g_stage[it.world().get_stage_id()];
	while (it.next()) {
		++out.tables;
		const auto wt = it.field<const WorldTransform>(0);
		const auto cube = it.field<const CubePrimitive>(1);
		const Material* inline_mat = it.is_set(2) ? &it.field<const Material>(2)[0] : nullptr;
		DrawItem* items = out.items.Grow(it.count());
		for (const auto i : it) {
			const DrawMaterial m = inline_mat != nullptr ? MaterialOf(inline_mat[i])
														 : MaterialOfEntity(it.entity(i).target<RenderWith>());
			WriteItem(items[i], wt[i], cube[i], it.entity(i).id(), m);
		}
	}
}

// One table slice, with the RenderWith target resolved once per table through a query term.
void ExtractPerTable(flecs::iter& it) {
	PaddedBuffer& out = g_stage[it.world().get_stage_id()];
	while (it.next()) {
		++out.tables;
		const auto wt = it.field<const WorldTransform>(0);
		const auto cube = it.field<const CubePrimitive>(1);
		const Material* inline_mat = it.is_set(2) ? &it.field<const Material>(2)[0] : nullptr;
		DrawMaterial shared{};
		if (inline_mat == nullptr && it.is_set(3)) shared = MaterialOfEntity(it.pair(3).second());
		DrawItem* items = out.items.Grow(it.count());
		for (const auto i : it) {
			WriteItem(items[i], wt[i], cube[i], it.entity(i).id(), inline_mat != nullptr ? MaterialOf(inline_mat[i]) : shared);
		}
	}
}

// The base query, with inherited visibility looked up by hand: one walk up ChildOf per table.
void ExtractSearchHidden(flecs::iter& it) {
	PaddedBuffer& out = g_stage[it.world().get_stage_id()];
	const flecs::world world = it.world();
	const ecs_world_t* real = ecs_get_world(world.c_ptr());
	const ecs_id_t hidden = world.id<Hidden>().raw_id();
	while (it.next()) {
		if (ecs_search_relation(real, it.c_ptr()->table, 0, hidden, EcsChildOf, EcsSelf | EcsUp, nullptr, nullptr, nullptr) >= 0) {
			continue;
		}
		const auto wt = it.field<const WorldTransform>(0);
		const auto cube = it.field<const CubePrimitive>(1);
		const Material* inline_mat = it.is_set(2) ? &it.field<const Material>(2)[0] : nullptr;
		DrawMaterial shared{};
		if (inline_mat == nullptr && it.is_set(3)) shared = MaterialOfEntity(it.pair(3).second());
		DrawItem* items = out.items.Grow(it.count());
		for (const auto i : it) {
			WriteItem(items[i], wt[i], cube[i], it.entity(i).id(), inline_mat != nullptr ? MaterialOf(inline_mat[i]) : shared);
		}
	}
}

void ResetStages() {
	for (PaddedBuffer& s : g_stage) {
		s.items.size = 0;
		s.tables = 0;
	}
}

// Joins the per-stage buffers into one list, as the seam needs.
void Join() {
	g_joined.size = 0;
	for (const PaddedBuffer& s : g_stage) {
		if (s.items.size == 0) continue;
		std::memcpy(g_joined.Grow(s.items.size), s.items.data.get(), s.items.size * sizeof(DrawItem));
	}
}

// Order independent, exact: whole numbers only.
double Checksum(const ItemBuffer& list) {
	double sum = 0.0;
	for (size_t i = 0; i < list.size; ++i) {
		const DrawItem& d = list.data[i];
		sum += static_cast<double>(static_cast<uint32_t>(d.entity) % 100003);
		sum += std::floor(d.bounds.x * 16.0F) + std::floor(d.bounds.z * 16.0F) + d.material.unlit * 3 + d.flags;
		sum += static_cast<double>(d.material.double_sided) * 5 + d.material.tint.r;
	}
	return sum;
}

// ── Timing ──────────────────────────────────────────────────────────────────────────────────────

struct Recorder {
	const Options& opt;
	std::map<std::string, std::vector<double>> samples;
	std::vector<std::string> order;
	std::vector<char> junk;
	bool recording{false};

	explicit Recorder(const Options& o) : opt(o) {
		if (opt.cold) junk.assign(96U * 1024U * 1024U, 1);
	}

	// Evicts the caches between steps in --cold runs (the machine's L3 is 32 MB per half).
	void Evict() {
		if (!opt.cold) return;
		for (size_t i = 0; i < junk.size(); i += 64) ++junk[i];
	}

	template<typename F>
	void Time(const std::string& name, F&& f) {
		Evict();
		const auto t0 = Clock::now();
		f();
		const auto t1 = Clock::now();
		if (!recording) return;
		auto [it, fresh] = samples.try_emplace(name);
		if (fresh) order.push_back(name);
		it->second.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
	}

	void Print() {
		const std::string flags = std::string(opt.cold ? "cold" : "warm") + (opt.physics ? "+physics" : "");
		for (const std::string& name : order) {
			auto& s = samples[name];
			std::ranges::sort(s);
			std::printf(
				"%s\t%d\t%d\t%s\t%s\t%.1f\t%.1f\t%.1f\n",
				opt.scene.c_str(),
				opt.entities,
				opt.threads,
				flags.c_str(),
				name.c_str(),
				s[s.size() / 2],
				s[s.size() * 95 / 100],
				s.front()
			);
		}
	}
};

// A copy of `src` with every `stride`-th item moved, and, for churn, every 100th item replaced by a new key and the
// order disturbed the way a table's swap-remove would.
ItemBuffer Variant(const ItemBuffer& src, const size_t stride, const bool churn) {
	ItemBuffer out;
	std::memcpy(out.Grow(src.size), src.data.get(), src.size * sizeof(DrawItem));
	for (size_t i = 0; stride > 0 && i < out.size; i += stride) out.data[i].transform[3].x += 0.01F;
	if (churn) {
		for (size_t i = 0; i < out.size; i += 100) {
			out.data[i] = out.data[out.size - 1 - i / 100];
			out.data[out.size - 1 - i / 100].entity += (1ULL << 40);
		}
	}
	return out;
}

int Run(const Options& opt) {
	spdlog::set_level(spdlog::level::err);
	const auto ctx = std::make_unique<engine::EngineContext>(nullptr);
	flecs::world& world = *ctx->GetWorld();
	ctx->GetModuleRegistry().Import(world, {"physics"});
	world.component<Hidden>();
	world.component<OtherTag>();
	world.component<Mover>();
	world.component<SpanJob>();
	world.component<ViewJob>();

	Builder builder(world, opt.entities);
	if (!builder.Build(opt.scene)) {
		std::fprintf(stderr, "unknown scene %s\n", opt.scene.c_str());
		return 1;
	}
	world.entity().set<Transform>({.position = {0, 15, 40}}).set<engine::render::Camera>({});
	if (opt.physics) {
		world.entity()
			.set<CollisionShape>({.box_half_extents = {50, 0.5F, 50}})
			.set<RigidBody>({.motion_type = engine::physics::MotionType::Static})
			.set<Transform>({.position = {0, -20, 0}});
		for (int i = 0; i < opt.bodies; ++i) {
			world.entity()
				.set<CollisionShape>({})
				.set<RigidBody>({})
				.set<Transform>({.position = {i % 10 * 1.05F, -15 + i / 100 * 1.2F, i / 10 % 10 * 1.05F}});
		}
	}
	if (opt.threads > 0) world.set_threads(opt.threads);

	// Systems under test, one pipeline each so a run's time includes waking and joining the workers.
	const auto per_entity = world.system<const WorldTransform, const CubePrimitive>("ExtractPerEntity")
								.kind(0)
								.with<const Material>()
								.self()
								.optional()
								.without<Translucent>()
								.multi_threaded(true)
								.run(ExtractPerEntity);
	per_entity.add<TagPerEntity>();
	const auto per_table = world.system<const WorldTransform, const CubePrimitive>("ExtractPerTable")
							   .kind(0)
							   .with<const Material>()
							   .self()
							   .optional()
							   .with<RenderWith>(flecs::Wildcard)
							   .self()
							   .up(flecs::ChildOf)
							   .optional()
							   .without<Translucent>()
							   .multi_threaded(true)
							   .run(ExtractPerTable);
	per_table.add<TagPerTable>();
	const auto per_table_hidden = world.system<const WorldTransform, const CubePrimitive>("ExtractPerTableHidden")
									  .kind(0)
									  .with<const Material>()
									  .self()
									  .optional()
									  .with<RenderWith>(flecs::Wildcard)
									  .self()
									  .up(flecs::ChildOf)
									  .optional()
									  .without<Translucent>()
									  .without<Hidden>()
									  .self()
									  .up(flecs::ChildOf)
									  .multi_threaded(true)
									  .run(ExtractPerTable);
	per_table_hidden.add<TagPerTableHidden>();
	const auto hidden_uncached = world.system<const WorldTransform, const CubePrimitive>("ExtractHiddenUncached")
									 .kind(0)
									 .with<const Material>()
									 .self()
									 .optional()
									 .with<RenderWith>(flecs::Wildcard)
									 .self()
									 .up(flecs::ChildOf)
									 .optional()
									 .without<Translucent>()
									 .without<Hidden>()
									 .self()
									 .up(flecs::ChildOf)
									 .cache_kind(flecs::QueryCacheNone)
									 .multi_threaded(true)
									 .run(ExtractPerTable);
	hidden_uncached.add<TagHiddenUncached>();
	const auto hidden_search = world.system<const WorldTransform, const CubePrimitive>("ExtractHiddenSearch")
								   .kind(0)
								   .with<const Material>()
								   .self()
								   .optional()
								   .with<RenderWith>(flecs::Wildcard)
								   .self()
								   .up(flecs::ChildOf)
								   .optional()
								   .without<Translucent>()
								   .multi_threaded(true)
								   .run(ExtractSearchHidden);
	hidden_search.add<TagHiddenSearch>();
	// Spans: the main thread gathers table slices, workers split them by entity count.
	const auto span_query = world.query_builder<const WorldTransform, const CubePrimitive>()
								.with<const Material>()
								.self()
								.optional()
								.with<RenderWith>(flecs::Wildcard)
								.self()
								.up(flecs::ChildOf)
								.optional()
								.without<Translucent>()
								.cached()
								.build();
	const int jobs = std::max(1, opt.threads);
	for (int i = 0; i < jobs; ++i) world.entity().set<SpanJob>({i});
	const auto span_work = world.system<const SpanJob>("ExtractSpans")
							   .kind(0)
							   .multi_threaded(true)
							   .each([jobs](const SpanJob& job) {
								   const size_t begin = g_joined.size * job.index / jobs;
								   const size_t end = g_joined.size * (job.index + 1) / jobs;
								   auto it = std::ranges::upper_bound(g_spans, begin, {}, &Span::first);
								   for (--it; it != g_spans.end() && it->first < end; ++it) {
									   const size_t lo = std::max(begin, it->first) - it->first;
									   const size_t hi = std::min(end, it->first + it->count) - it->first;
									   DrawItem* out = g_joined.data.get() + it->first;
									   for (size_t i = lo; i < hi; ++i) {
										   const DrawMaterial m =
											   it->inline_mat != nullptr ? MaterialOf(it->inline_mat[i]) : it->shared;
										   WriteItem(out[i], it->wt[i], it->cube[i], it->entities[i], m);
									   }
								   }
							   });
	span_work.add<TagSpanWork>();
	const auto empty = world.system<const SpanJob>("Empty").kind(0).multi_threaded(true).each([](const SpanJob&) {});
	empty.add<TagEmpty>();
	for (int i = 0; i < 6; ++i) world.entity().set<ViewJob>({i});
	const auto views = world.system<const ViewJob>("Views").kind(0).multi_threaded(true).each([](const ViewJob& v) {
		CullAndSortKeys(g_views[v.index], g_joined);
	});
	views.add<TagViews>();
	const flecs::entity render_cubes = FindSystem(world, "RenderCubes");
	render_cubes.add<TagRenderCubes>();

	const flecs::entity pipe_empty = MakePipeline<TagEmpty>(world);
	const flecs::entity pipe_entity = MakePipeline<TagPerEntity>(world);
	const flecs::entity pipe_table = MakePipeline<TagPerTable>(world);
	const flecs::entity pipe_hidden = MakePipeline<TagPerTableHidden>(world);
	const flecs::entity pipe_uncached = MakePipeline<TagHiddenUncached>(world);
	const flecs::entity pipe_search = MakePipeline<TagHiddenSearch>(world);
	const flecs::entity pipe_spans = MakePipeline<TagSpanWork>(world);
	const flecs::entity pipe_cubes = MakePipeline<TagRenderCubes>(world);
	const flecs::entity pipe_views = MakePipeline<TagViews>(world);
	const flecs::entity propagation = FindSystem(world, "TransformPropagation");
	const flecs::entity begin = FindSystem(world, "Render3DBegin");
	const flecs::entity end = FindSystem(world, "Render3DEnd");
	const flecs::entity step = FindSystem(world, "PhysicsStep");
	const auto movers = world.query<Transform, const Mover>();
	BuildViews();

	// One RenderScene per case and lookup: each must see only its own pair of lists, alternating.
	RenderScene still_map(Lookup::StdMap);
	RenderScene still_flat(Lookup::Flat);
	RenderScene still_hint(Lookup::Hint);
	RenderScene tenth_flat(Lookup::Flat);
	RenderScene tenth_hint(Lookup::Hint);
	RenderScene all_flat(Lookup::Flat);
	RenderScene all_hint(Lookup::Hint);
	RenderScene churn_flat(Lookup::Flat);
	RenderScene churn_hint(Lookup::Hint);
	Recorder rec(opt);
	const int warmup = 40;
	size_t tables = 0;
	size_t biggest_stage = 0;
	std::array<ItemBuffer, 2> still;
	std::array<ItemBuffer, 2> tenth;
	std::array<ItemBuffer, 2> all;
	std::array<ItemBuffer, 2> churn;

	for (int frame = 0; frame < warmup + opt.iters; ++frame) {
		rec.recording = frame >= warmup;
		movers.each([&](Transform& t, const Mover&) { t.rotation.y += 0.01F; });
		if (opt.physics) rec.Time("physics_step", [&] { world.system(step).run(1.0F / 60.0F); });

		rec.Time("propagation", [&] { world.system(propagation).run(1.0F / 60.0F); });

		world.system(begin).run();
		rec.Time("today_render_cubes", [&] { world.run_pipeline(pipe_cubes); });
		world.get_mut<engine::render::RenderQueue>().commands.clear();
		world.system(end).run();

		rec.Time("sync_only", [&] { world.run_pipeline(pipe_empty); });

		ResetStages();
		rec.Time("extract_per_entity", [&] { world.run_pipeline(pipe_entity); });
		ResetStages();
		rec.Time("extract_per_table", [&] { world.run_pipeline(pipe_table); });
		biggest_stage = 0;
		for (const PaddedBuffer& s : g_stage) {
			biggest_stage = std::max(biggest_stage, s.items.size);
			tables += frame == 0 ? s.tables : 0;
		}
		rec.Time("join", [&] { Join(); });
		const size_t extracted = g_joined.size;
		const double checksum = frame == 0 ? Checksum(g_joined) : 0.0;
		ResetStages();
		rec.Time("extract_per_table_hidden", [&] { world.run_pipeline(pipe_hidden); });
		ResetStages();
		rec.Time("extract_hidden_uncached", [&] { world.run_pipeline(pipe_uncached); });
		ResetStages();
		rec.Time("extract_hidden_search", [&] { world.run_pipeline(pipe_search); });

		rec.Time("extract_spans", [&] {
			g_spans.clear();
			size_t total = 0;
			span_query.run([&](flecs::iter& it) {
				while (it.next()) {
					const bool has_inline = it.is_set(2);
					g_spans.push_back(
						{.wt = &it.field<const WorldTransform>(0)[0],
						 .cube = &it.field<const CubePrimitive>(1)[0],
						 .inline_mat = has_inline ? &it.field<const Material>(2)[0] : nullptr,
						 .entities = it.c_ptr()->entities,
						 .shared = !has_inline && it.is_set(3) ? MaterialOfEntity(it.pair(3).second()) : DrawMaterial{},
						 .count = it.count(),
						 .first = total}
					);
					total += it.count();
				}
			});
			g_joined.size = 0;
			g_joined.Grow(total);
			world.run_pipeline(pipe_spans);
		});
		if (frame == 0) {
			const double b = Checksum(g_joined);
			if (checksum != b) {
				std::fprintf(stderr, "the two extractions differ: %f %f\n", checksum, b);
				return 1;
			}
		}
		if (g_joined.size != extracted || extracted == 0) {
			std::fprintf(stderr, "extracted %zu items, spans %zu\n", extracted, g_joined.size);
			return 1;
		}

		rec.Time("cull_only_1", [&] { CullOnly(g_views[0], g_joined); });
		rec.Time("cull_only_6", [&] { for (View& v : g_views) CullOnly(v, g_joined); });
		rec.Time("cull_group_1", [&] { CullAndGroup(g_views[0], g_joined); });
		rec.Time("cull_group_6", [&] { for (View& v : g_views) CullAndGroup(v, g_joined); });
		rec.Time("cull_keys_1", [&] { CullAndSortKeys(g_views[0], g_joined); });
		rec.Time("cull_keys_6", [&] { for (View& v : g_views) CullAndSortKeys(v, g_joined); });
		rec.Time("cull_keys_6_workers", [&] { world.run_pipeline(pipe_views); });
		rec.Time("write_instances_6", [&] { for (View& v : g_views) WriteInstances(v, g_joined); });

		if (frame == 0) {
			for (int i = 0; i < 2; ++i) {
				still[i] = Variant(g_joined, 0, false);
				tenth[i] = Variant(g_joined, i == 0 ? 0 : 10, false);
				all[i] = Variant(g_joined, i == 0 ? 0 : 1, false);
				churn[i] = Variant(g_joined, i == 0 ? 0 : 10, i == 1);
			}
		}
		const int side = frame & 1;
		rec.Time("scene_still_map", [&] { still_map.Compare(still[side]); });
		rec.Time("scene_still_flat", [&] { still_flat.Compare(still[side]); });
		rec.Time("scene_still_hint", [&] { still_hint.Compare(still[side]); });
		rec.Time("scene_tenth_flat", [&] { tenth_flat.Compare(tenth[side]); });
		rec.Time("scene_tenth_hint", [&] { tenth_hint.Compare(tenth[side]); });
		rec.Time("scene_all_flat", [&] { all_flat.Compare(all[side]); });
		rec.Time("scene_all_hint", [&] { all_hint.Compare(all[side]); });
		rec.Time("scene_churn_flat", [&] { churn_flat.Compare(churn[side]); });
		rec.Time("scene_churn_hint", [&] { churn_hint.Compare(churn[side]); });
	}

	// Hiding one moving parent drops its drawn children. What a toggle costs the next extraction (rematching), against
	// the same frame without one, and the same for spawning one group and destroying the last.
	const flecs::entity hide = world.query<const Mover>().first();
	if (hide) {
		const auto each_variant = [&](const std::string& what, const bool print) {
			const std::array<std::pair<const char*, flecs::entity>, 4> pipes{
				{{"per_table", pipe_table}, {"hidden", pipe_hidden}, {"hidden_uncached", pipe_uncached}, {"hidden_search", pipe_search}}
			};
			for (const auto& [name, pipe] : pipes) {
				ResetStages();
				rec.Time(what + "_" + name, [&] { world.run_pipeline(pipe); });
				Join();
				if (print) std::printf("# %s %s: %zu items\n", what.c_str(), name, g_joined.size);
			}
			rec.Time(what + "_propagation", [&] { world.system(propagation).run(1.0F / 60.0F); });
		};
		for (int i = 0; i < 60; ++i) {
			rec.recording = i >= 10;
			if (i % 2 == 0) flecs::entity(hide).add<Hidden>();
			else flecs::entity(hide).remove<Hidden>();
			each_variant("toggle", i == 0);
		}
		for (int i = 0; i < 60; ++i) {
			rec.recording = i >= 10;
			if (i % 2 == 0) flecs::entity(hide).add<OtherTag>();
			else flecs::entity(hide).remove<OtherTag>();
			each_variant("othertag", false);
		}
		flecs::entity last;
		for (int i = 0; i < 60; ++i) {
			rec.recording = i >= 10;
			if (last) last.destruct();
			const int placed = builder.placed;
			last = world.entity().child_of(builder.root).set<Transform>({}).add<Mover>();
			for (int c = 0; c < 4; ++c) {
				world.entity().child_of(last).set<Transform>({}).set<CubePrimitive>({}).add<RenderWith>(builder.materials[0]);
			}
			builder.placed = placed;
			each_variant("spawn", false);
		}
	}

	rec.Print();
	std::printf(
		"# %s %d threads=%d: items %zu, tables %zu, largest stage %zu, sizeof(DrawItem) %zu, visible",
		opt.scene.c_str(),
		opt.entities,
		opt.threads,
		g_joined.size,
		tables,
		biggest_stage,
		sizeof(DrawItem)
	);
	for (const View& v : g_views) std::printf(" %zu", v.visible);
	std::printf(", batches");
	for (const View& v : g_views) std::printf(" %zu", v.groups.size());
	const auto report = [](const char* name, const RenderScene& s) {
		std::printf(", %s +%zu ~%zu -%zu", name, s.changes.added.size(), s.changes.moved.size(), s.changes.removed.size());
	};
	report("still", still_hint);
	report("tenth", tenth_hint);
	report("all", all_hint);
	report("churn flat", churn_flat);
	report("churn hint", churn_hint);
	std::printf("\n");
	return 0;
}

} // namespace

int main(const int argc, char** argv) {
	Options opt;
	for (int i = 1; i < argc; ++i) {
		const std::string a = argv[i];
		if (a == "--cold") opt.cold = true;
		else if (a == "--physics") opt.physics = true;
		else if (i + 1 < argc && a == "--scene") opt.scene = argv[++i];
		else if (i + 1 < argc && a == "--entities") opt.entities = std::atoi(argv[++i]);
		else if (i + 1 < argc && a == "--threads") opt.threads = std::atoi(argv[++i]);
		else if (i + 1 < argc && a == "--iters") opt.iters = std::atoi(argv[++i]);
		else if (i + 1 < argc && a == "--bodies") opt.bodies = std::atoi(argv[++i]);
		else {
			std::fprintf(stderr, "unknown argument %s\n", a.c_str());
			return 2;
		}
	}
	return Run(opt);
}
