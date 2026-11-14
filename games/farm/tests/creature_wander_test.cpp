/// Runtime-behaviour test for the creature wander action (ticket 10). Exercises the real
/// "creature_wander" BT action handler registered by RegisterCreatureSystems against a small tilemap
/// with blocked tiles, asserting the movement invariants that only surface at runtime: a creature
/// never steps out of bounds, onto a blocked tile, or beyond its wander radius — yet still moves.
/// No GPU/scene needed (the engine Flecs REST API is not enabled in this build).
#include <gtest/gtest.h>

#include <cmath>

#include <flecs.h>

#include "engine/engine.hpp"
#include "farm/creature_components.hpp"
#include "farm/creature_system.hpp"

using namespace game::farm;
using namespace engine;

namespace {

// A w×h tilemap with no TileSet (so the handler falls back to bounds + blocked-override checks),
// with the given list of tiles marked blocked.
tilemap::Tilemap MakeTilemap(const int w, const int h, const std::vector<glm::ivec2>& blocked) {
	tilemap::Tilemap tm;
	tm.width = static_cast<uint32_t>(w);
	tm.height = static_cast<uint32_t>(h);
	tm.tile_ids.assign(static_cast<size_t>(w) * h, 0);
	tm.blocked_override.assign(static_cast<size_t>(w) * h, 0);
	for (const glm::ivec2 b : blocked) {
		tm.blocked_override[static_cast<size_t>(b.y) * w + b.x] = 1;
	}
	return tm;
}

} // namespace

TEST(CreatureWanderTest, creature_stays_in_bounds_radius_and_off_blocked_tiles) {
	flecs::world world;

	const glm::ivec2 home{10, 10};
	const int radius = 3;

	// Block several tiles inside the wander box to prove the creature steps around them.
	const std::vector<glm::ivec2> blocked{{9, 10}, {11, 10}, {10, 9}, {10, 11}, {8, 8}, {12, 12}};
	const flecs::entity tilemapEnt = world.entity("Tilemap");
	tilemapEnt.set<tilemap::Tilemap>(MakeTilemap(21, 21, blocked));

	// Registers the "creature_wander" action into the BTHandlerRegistry (capturing the tilemap). The
	// engine "ai" module normally sets this singleton; create it directly in this bare test world.
	world.set<ai::BTHandlerRegistry>({});
	RegisterCreatureSystems(world, tilemapEnt);
	const auto& reg = world.get<ai::BTHandlerRegistry>();
	const auto it = reg.actions.find("creature_wander");
	ASSERT_NE(it, reg.actions.end());
	const ai::BTActionHandler& wander = it->second;

	const flecs::entity creature = world.entity();
	creature.set<CreatureWander>({.home = home, .radius = radius, .moveInterval = 0.5f, .timer = 0.0f, .rng = 12345u});
	creature.set<tilemap::GridPosition>({.x = home.x, .z = home.y});

	ai::Blackboard bb;
	auto isBlocked = [&](const int x, const int z) {
		for (const glm::ivec2 b : blocked) {
			if (b.x == x && b.y == z) {
				return true;
			}
		}
		return false;
	};

	bool movedAtLeastOnce = false;
	for (int tick = 0; tick < 400; ++tick) {
		// dt comfortably exceeds moveInterval so every tick is an amble beat.
		const ai::BTStatus status = wander(creature, bb, 1.0f);
		EXPECT_EQ(status, ai::BTStatus::Running); // the wander repeater never completes

		const auto& gp = creature.get<tilemap::GridPosition>();
		EXPECT_GE(gp.x, 0);
		EXPECT_GE(gp.z, 0);
		EXPECT_LT(gp.x, 21);
		EXPECT_LT(gp.z, 21);
		EXPECT_LE(std::abs(gp.x - home.x), radius) << "strayed beyond wander radius in x";
		EXPECT_LE(std::abs(gp.z - home.y), radius) << "strayed beyond wander radius in z";
		EXPECT_FALSE(isBlocked(gp.x, gp.z)) << "stepped onto a blocked tile at " << gp.x << "," << gp.z;

		if (gp.x != home.x || gp.z != home.y) {
			movedAtLeastOnce = true;
		}
	}
	EXPECT_TRUE(movedAtLeastOnce) << "the creature never moved from its home tile";
}

TEST(CreatureWanderTest, fully_boxed_in_creature_never_moves) {
	flecs::world world;

	const glm::ivec2 home{5, 5};
	// Surround the home tile on all 8 neighbours so no step is ever legal.
	std::vector<glm::ivec2> blocked;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx != 0 || dy != 0) {
				blocked.push_back({home.x + dx, home.y + dy});
			}
		}
	}

	const flecs::entity tilemapEnt = world.entity("Tilemap");
	tilemapEnt.set<tilemap::Tilemap>(MakeTilemap(11, 11, blocked));
	world.set<ai::BTHandlerRegistry>({});
	RegisterCreatureSystems(world, tilemapEnt);
	const ai::BTActionHandler& wander = world.get<ai::BTHandlerRegistry>().actions.at("creature_wander");

	const flecs::entity creature = world.entity();
	creature.set<CreatureWander>({.home = home, .radius = 2, .moveInterval = 0.1f, .timer = 0.0f, .rng = 777u});
	creature.set<tilemap::GridPosition>({.x = home.x, .z = home.y});

	ai::Blackboard bb;
	for (int tick = 0; tick < 100; ++tick) {
		wander(creature, bb, 1.0f);
		const auto& gp = creature.get<tilemap::GridPosition>();
		EXPECT_EQ(gp.x, home.x);
		EXPECT_EQ(gp.z, home.y);
	}
}
