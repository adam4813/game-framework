#include "object_kind_registry.hpp"

#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "engine/engine.hpp"

namespace game::farm {

ObjectKindRegistry ParseObjectKindRegistry(const nlohmann::json& doc) {
	ObjectKindRegistry registry;

	if (!doc.contains("objectKinds") || !doc["objectKinds"].is_array()) {
		return registry;
	}

	for (const auto& entry : doc["objectKinds"]) {
		ObjectKindDef def{
			.kind = entry.value("kind", ""),
			.height = entry.value("height", 0.0f),
			.sprite = entry.value("sprite", ""),
			.blocksMovement = entry.value("blocksMovement", false),
		};

		if (!def.kind.empty()) {
			registry.defs[def.kind] = std::move(def);
		}
	}

	return registry;
}

void LoadObjectKindRegistry(flecs::world& world) {
	ObjectKindRegistry registry;

	const auto* ref = world.try_get<engine::platform::PlatformRef>();
	const std::string path = (ref && ref->ptr) ? (ref->ptr->AssetDirectory() + "/data/object_kinds.json") : "";

	if (!path.empty()) {
		if (const auto j = engine::core::LoadJsonFile(path, "ObjectKindRegistry")) {
			registry = ParseObjectKindRegistry(*j);
			engine::ecs::Log(world).info("[ObjectKindRegistry] Loaded {} object kind(s)", registry.defs.size());
		}
	}

	world.set<ObjectKindRegistry>(std::move(registry));
}

} // namespace game::farm
