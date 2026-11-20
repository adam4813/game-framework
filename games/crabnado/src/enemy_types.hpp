#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <flecs.h>

#include "campaign.hpp"
#include "crabnado_constants.hpp"
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
	// The ride, immunity, wind-up and charge levers (see crabnado_constants.hpp). `charges` false skips the wind-up
	// and charge, so the type just resumes chasing after a ride.
	float ride_base{constants::kRideBase};
	float ride_per_height{constants::kRidePerHeight};
	float ride_max{constants::kRideMax};
	float ride_push{constants::kRidePush};
	float immune{constants::kImmune};
	float windup{constants::kWindup};
	float charge_mult{constants::kChargeMult};
	float charge_time{constants::kChargeTime};
	float charge_dps_mult{constants::kChargeDpsMult};
	bool charges{true};
	// A surfer (the seahorse) is carried by a crest it joins and steers along it, never ridden, stunned or charged. Its
	// contact damage is multiplied by surf_dps_mult while it surfs, it steers across the crest at surf_steer units per
	// second and it swims at pool_speed in a tide pool. `tumble_min` is the crest height that grabs and keeps it.
	bool surfs{false};
	float surf_dps_mult{1.0F};
	float surf_steer{5.0F};
	float pool_speed{3.0F};
};

// Singleton: every enemy type, in id order. An Enemy's `type` indexes into it.
struct EnemyRegistry {
	std::vector<EnemyType> types;
};

// Replaces the EnemyRegistry singleton with the types in the JSON file at `path`. A malformed entry is
// logged and skipped.
void LoadEnemyTypes(const flecs::world& world, std::string_view path);

// Seconds a crest of `peak` height carries an enemy of `type`: ride_base + ride_per_height * (peak - tumble_min),
// capped at ride_max. A ride's peak only ever rises, so a taller crest mid-ride extends it but never restarts it.
[[nodiscard]] float RideDuration(const EnemyType& type, float peak);

// Index of an enemy type drawn by weight from the spawn table entries whose spawner is `source` (what a Stage's spawn
// table lists, see campaign.hpp), or -1 if the table has none.
[[nodiscard]] int PickEnemyType(const flecs::world& world, const std::vector<SpawnEntry>& spawns, std::string_view source);

} // namespace crabnado
