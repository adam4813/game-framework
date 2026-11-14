#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include <flecs.h>
#include <nlohmann/json_fwd.hpp>

namespace game::farm {

// Data-driven definition of one world-object kind, loaded from games/farm/assets/data/object_kinds.json.
// `kind` is the open-taxonomy identity key; later tickets (04 buildings, 05 trees, 10 creatures)
// add entries with zero C++ change. This mirrors the CropRegistry pattern: a string-keyed singleton
// loaded at the scene/caller boundary and consumed by rendering/placement — never by the pure
// generation layer.
struct ObjectKindDef {
	std::string kind;           // registry key, e.g. "tree_oak", "house"
	float height{0.0f};         // default visual height in world units
	std::string sprite;         // sprite-atlas frame reference used to render this kind
	bool blocksMovement{false}; // default collision for objects of this kind

	friend bool operator==(const ObjectKindDef&, const ObjectKindDef&) = default;
};

// Singleton registry of all loaded object-kind definitions, keyed by `kind`.
struct ObjectKindRegistry {
	std::unordered_map<std::string, ObjectKindDef> defs;
};

// Look up a definition by kind key, or nullptr when absent.
[[nodiscard]] inline const ObjectKindDef* FindObjectKindDef(const ObjectKindRegistry& reg, std::string_view kind) {
	const auto it = reg.defs.find(std::string{kind});
	return (it != reg.defs.end()) ? &it->second : nullptr;
}

// Pure parse of an object-kinds JSON document (the "objectKinds" array) into a registry. File-free
// so it is unit-testable without Platform. Missing fields fall back to defaults; a missing or
// non-array "objectKinds" yields an empty registry.
[[nodiscard]] ObjectKindRegistry ParseObjectKindRegistry(const nlohmann::json& doc);

// Load games/farm/assets/data/object_kinds.json via Platform and set the ObjectKindRegistry singleton. An
// empty registry is set when the file is missing so downstream lookups stay safe.
void LoadObjectKindRegistry(flecs::world& world);

} // namespace game::farm
