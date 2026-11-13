#pragma once

#include <span>
#include <vector>

#include <glm/glm.hpp>

#include <flecs.h>

#include "waves.hpp"

// The arena: a circle of open ocean with rocks and tide pools, walled by a glowing magic barrier, loaded from level
// JSON. Pools are placed by hand; rocks are scattered from the run's seed by the level's "rock_scatter" (hand-placed
// rocks still work). Rocks and pools are entities (a Transform plus Rock or PoolSite); the per-frame code reads the
// Rocks and TidePools caches built after the level loads, so it never queries entities.
namespace crabnado {

// Level component on a rock entity: it blocks the player and landed enemies within `radius`.
struct Rock {
	float radius{0.0F};
};

// Level component on a tide pool entity: waves fade within `radius` of its Transform.
struct PoolSite {
	float radius{0.0F};
};

// Singleton from the level's "arena_edge": the arena's radius and how many rune pillars stand on its barrier.
struct ArenaEdge {
	float radius{0.0F};
	int pillars{0};
};

// One band of scattered rocks: `count` rocks centred between min_distance and max_distance from the arena centre,
// each min_radius to max_radius across. Distance is drawn uniformly, so a band crowds toward its inner edge.
struct RockBand {
	int count{0};
	float min_distance{0.0F};
	float max_distance{0.0F};
	float min_radius{0.0F};
	float max_radius{0.0F};
};

// Singleton from the level's "rock_scatter": the bands rocks are rolled into each run. Rocks keep `gap` clear of
// each other and of pools, so crabs can pass between them.
struct RockScatter {
	RockBand inner;
	RockBand outer;
	float gap{0.0F};
};

// A piece of the barrier that glows and bobs with the BarrierShimmer system: `angle` is its bearing around the
// ring, `rest_y` its height at rest and `bob` how far it rises and falls.
struct BarrierGlow {
	float angle{0.0F};
	float rest_y{0.0F};
	float bob{0.0F};
};

struct RockCircle {
	glm::vec2 center{0.0F};
	float radius{0.0F};
};

// Singletons: the rocks and pools of the loaded level.
struct Rocks {
	std::vector<RockCircle> circles;
};
struct TidePools {
	std::vector<TidePool> pools;
};

// Registers the arena components, the BarrierShimmer system and the level loaders (rock, tide_pool, arena_edge,
// rock_scatter). Call once, before the arena scene loads.
void RegisterArena(const flecs::world& world);

// Loads the arena level as children of `parent`, scatters its rocks from RngState, dresses it (rocks, barrier) and
// fills the Rocks and TidePools caches. `keepClear` are points (with a radius) no rock may cover. The caller must not be deferring (see ArenaScene::BuildStage), or the level's entities won't exist yet.
// Call before SpawnWaterTiles, which tints the pools.
void BuildArena(const flecs::world& world, flecs::entity parent, std::span<const RockCircle> keepClear);

// Empties the Rocks and TidePools caches (the level entities go with the scene root).
void ClearArena(const flecs::world& world);

// Pushes a circle at `position` out of any rock it overlaps. Returns whether it hit one.
bool PushOutOfRocks(std::span<const RockCircle> rocks, glm::vec2& position, float radius);

} // namespace crabnado
