#pragma once

#include <functional>
#include <string>
#include <vector>

#include <flecs.h>
#include <nlohmann/json.hpp>

// Plain-data holders that back the save registry. These describe *what* is saved and *how* a
// value maps to/from ECS data — they carry no game-specific logic themselves. The fluent builders
// in save_registry.hpp populate these; SaveToJson / LoadFromJson consume them.
namespace engine::save {

using Json = nlohmann::json;

// A single keyed value in the save file. `key` is a dotted path (e.g. "economy.tokens") that maps
// to a nested JSON location. `get` reads the value out of the world; `set` writes it back on load.
// Either may be null (e.g. a save-only or load-only field) and is simply skipped.
struct SaveField {
	std::string key;
	std::function<Json(const flecs::world&)> get;
	std::function<void(flecs::world&, const Json&)> set;
};

// One member/property binding for a tag-based entity type. `save` writes the property into the
// entity's JSON object; `load` reads it back onto a freshly reconstructed entity.
struct EntityMemberBinding {
	std::string name;
	std::function<void(flecs::entity, Json&)> save;
	std::function<void(flecs::entity, const Json&)> load;
};

// A tag-based entity save type. Every entity carrying the tag is serialized to one entry in a JSON
// array under `key`. Entities are treated as ephemeral: on load, all currently tagged entities are
// destroyed and recreated from the array (no stable ids / matching).
//
// `collect` gathers the currently tagged entities; `create` spawns a new entity with the tag added.
// Both bake the C++ tag type into a lambda so the registry needs no world at registration time.
//
// Serialization is member-driven by default (the `members` list). `customSerialize` /
// `customDeserialize` are optional escape hatches: after the members run, a custom serializer's
// object is merged over the result (or replaces it wholesale if it returns a non-object), and a
// custom deserializer runs after the member loads.
struct EntitySaveType {
	std::string key;
	std::function<void(const flecs::world&, std::vector<flecs::entity>&)> collect;
	std::function<flecs::entity(flecs::world&)> create;
	std::vector<EntityMemberBinding> members;
	std::function<Json(flecs::entity)> customSerialize;
	std::function<void(flecs::entity, const Json&)> customDeserialize;
};

} // namespace engine::save
