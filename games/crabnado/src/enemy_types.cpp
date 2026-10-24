#include "enemy_types.hpp"

#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

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
		.appears_at = entry.value("appears_at", def.appears_at),
		.weight = entry.value("weight", def.weight)
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

int PickEnemyType(const flecs::world& world, const float clock) {
	const auto& types = world.get<EnemyRegistry>().types;
	std::vector<float> weights;
	weights.reserve(types.size());
	for (const EnemyType& type : types) {
		weights.push_back(type.appears_at <= clock ? type.weight : 0.0F);
	}
	return world.get_mut<ecs::RngState>().WeightedIndex(weights);
}

} // namespace crabnado
