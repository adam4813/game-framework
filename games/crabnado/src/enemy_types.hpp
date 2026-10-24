#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <flecs.h>

#include "engine/engine.hpp"

// The enemy-type registry: a string-keyed JSON file (crab, shrimp, lobster, ...) holding each type's stats, so a
// new type or a tuned value needs no C++. Loaded on every arena scene Load, so an edit shows on the next Play.
namespace crabnado {

struct EnemyType {
	std::string id;
	float hp{1.0F};
	float speed{1.0F};
	float radius{1.0F};
	float contact_dps{0.0F};
	int xp{1};
	float scale{1.0F};
	engine::core::Rgba color{.r = 255, .g = 255, .b = 255, .a = 255};
	bool walks_sideways{true};
	// Wave height above which a crest tumbles this type.
	float tumble_min{0.55F};
	// Run clock (seconds) from which the crabnado throws this type, and its weight in the weighted pick.
	float appears_at{0.0F};
	float weight{1.0F};
};

// Singleton: every enemy type, in id order. An Enemy's `type` indexes into it.
struct EnemyRegistry {
	std::vector<EnemyType> types;
};

// Replaces the EnemyRegistry singleton with the types in the JSON file at `path`. A malformed entry is
// logged and skipped.
void LoadEnemyTypes(const flecs::world& world, std::string_view path);

// Index of a type drawn by weight from those with appears_at <= `clock`, or -1 if none is unlocked.
[[nodiscard]] int PickEnemyType(const flecs::world& world, float clock);

} // namespace crabnado
