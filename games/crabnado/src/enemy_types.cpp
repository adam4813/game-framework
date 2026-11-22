#include "enemy_types.hpp"

#include <algorithm>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "campaign.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr std::string_view kLogTag = "EnemyTypes";

EnemyType ParseEnemyType(const std::string& id, const nlohmann::json& entry) {
	const EnemyType def;
	return {
		.id = id,
		.hp = entry.value("hp", def.hp),
		.speed = entry.value("speed", def.speed),
		.radius = entry.value("radius", def.radius),
		.contact_dps = entry.value("contact_dps", def.contact_dps),
		.xp = entry.value("xp", def.xp),
		.scale = entry.value("scale", def.scale),
		.color = core::JRgba(entry, "color", def.color),
		.walks_sideways = entry.value("walks_sideways", def.walks_sideways),
		.tumble_min = entry.value("tumble_min", def.tumble_min),
		.ride_base = entry.value("ride_base", def.ride_base),
		.ride_per_height = entry.value("ride_per_height", def.ride_per_height),
		.ride_max = entry.value("ride_max", def.ride_max),
		.ride_push = entry.value("ride_push", def.ride_push),
		.immune = entry.value("immune", def.immune),
		.windup = entry.value("windup", def.windup),
		.charge_mult = entry.value("charge_mult", def.charge_mult),
		.charge_time = entry.value("charge_time", def.charge_time),
		.charge_dps_mult = entry.value("charge_dps_mult", def.charge_dps_mult),
		.charges = entry.value("charges", def.charges),
		.surfs = entry.value("surfs", def.surfs),
		.surf_dps_mult = entry.value("surf_dps_mult", def.surf_dps_mult),
		.surf_steer = entry.value("surf_steer", def.surf_steer),
		.pool_speed = entry.value("pool_speed", def.pool_speed)
	};
}

} // namespace

void LoadEnemyTypes(const flecs::world& world, const std::string_view path) {
	EnemyRegistry registry;
	const auto doc = core::LoadJsonFile(path, kLogTag);
	if (!doc || !doc->is_object()) {
		if (doc) {
			spdlog::error("[{}] '{}' must be an object of enemy types keyed by id", kLogTag, path);
		}
	} else {
		for (const auto& [id, entry] : doc->items()) {
			if (!entry.is_object()) {
				spdlog::error("[{}] '{}' entry '{}' must be an object", kLogTag, path, id);
				continue;
			}
			registry.types.push_back(ParseEnemyType(id, entry));
		}
		spdlog::info("[{}] Loaded {} enemy types from '{}'", kLogTag, registry.types.size(), path);
	}
	world.set<EnemyRegistry>(std::move(registry));
}

float RideDuration(const EnemyType& type, const float peak) {
	return std::clamp(type.ride_base + type.ride_per_height * std::max(0.0F, peak - type.tumble_min), 0.0F, type.ride_max);
}

int PickEnemyType(const flecs::world& world, const std::vector<SpawnEntry>& spawns, const std::string_view source) {
	std::vector<float> weights;
	weights.reserve(spawns.size());
	for (const SpawnEntry& entry : spawns) {
		weights.push_back(entry.source == source ? entry.weight : 0.0F);
	}
	const int picked = world.get_mut<ecs::RngState>().WeightedIndex(weights);
	return picked < 0 ? -1 : spawns[static_cast<size_t>(picked)].type;
}

} // namespace crabnado
