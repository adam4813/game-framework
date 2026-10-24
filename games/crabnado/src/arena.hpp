#pragma once

#include <span>
#include <vector>

#include <glm/glm.hpp>

#include <flecs.h>

#include "waves.hpp"

// The arena: a circular shore with rocks, tide pools and a ring of driftwood, loaded from level JSON. Rocks and
// pools are entities (a Transform plus Rock or PoolSite); the per-frame code reads the Rocks and TidePools
// caches built after the level loads, so it never queries entities.
namespace crabnado {

// Level component on a rock entity: it blocks the player and landed enemies within `radius`.
struct Rock {
	float radius{0.0F};
};

// Level component on a tide pool entity: waves fade within `radius` of its Transform.
struct PoolSite {
	float radius{0.0F};
};

// Singleton from the level's "arena_edge": the shore's radius and how many driftwood boxes ring it.
struct ArenaEdge {
	float radius{0.0F};
	int driftwood{0};
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

// Registers the arena components and the level loaders (rock, tide_pool, arena_edge). Call once, before the arena scene loads.
void RegisterArena(const flecs::world& world);

// Loads the arena level as children of `parent`, dresses it (rocks, driftwood) and fills the Rocks and TidePools
// caches. The caller must not be deferring (see ArenaScene::BuildStage), or the level's entities won't exist yet.
// Call before SpawnWaterTiles, which tints the pools.
void BuildArena(const flecs::world& world, flecs::entity parent);

// Empties the Rocks and TidePools caches (the level entities go with the scene root).
void ClearArena(const flecs::world& world);

// Pushes a circle at `position` out of any rock it overlaps. Returns whether it hit one.
bool PushOutOfRocks(std::span<const RockCircle> rocks, glm::vec2& position, float radius);

} // namespace crabnado
