#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <flecs.h>
#include <nlohmann/json.hpp>

#include "save_components.hpp"

// The save registry: the developer-facing surface of the save system. It holds the schema — the
// explicit list of things that get saved and how they map to/from ECS data. Nothing is saved
// unless it is registered here, so the developer stays in full control (this is deliberately NOT a
// raw component dump).
//
// Two ways to declare a binding, mirroring each other:
//   * Member-pointer sugar — type-checked, auto bidirectional via nlohmann_json:
//       reg.Singleton<Economy>("economy")
//          .Member("tokens", &Economy::parlorTokens)
//          .Member("points", &Economy::scorePoints);
//       reg.Entities<Persist>("upgrades")
//          .Member("level", &Upgrade::level);
//   * Lambda escape hatch — for computed / cross-component values:
//       reg.Field("meta.playtime").Get(...).Set(...);
//       reg.Entities<Persist>("fx").Serialize(...).Deserialize(...);
//
// Member types must be JSON-convertible (arithmetic, bool, std::string, containers, or a type with
// nlohmann to_json/from_json). For anything else, use the lambda escape hatch.
namespace engine::save {

template<typename T>
class SingletonBinder;
class FieldBinder;
template<typename Tag>
class EntityBinder;

// Singleton holding the save schema. Registered by SaveModule; obtain it with
// `world.get_mut<SaveRegistry>()` and register bindings (typically during scene/module setup).
class SaveRegistry {
public:
	// Written to the top-level "version" key so loaders can migrate older saves.
	int version = 1;

	std::vector<SaveField> fields;
	std::vector<EntitySaveType> entityTypes;

	// Bind members of a singleton component under a dotted key prefix (pass "" for no prefix).
	template<typename T>
	SingletonBinder<T> Singleton(std::string keyPrefix);

	// Register a single custom keyed value, supplying Get/Set lambdas.
	FieldBinder Field(std::string key);

	// Register a tag-based entity type serialized to a JSON array under `key`.
	template<typename Tag>
	EntityBinder<Tag> Entities(std::string key);
};

// ── Builders (SaveRegistry is complete above) ──────────────────────────────────────────────────

// Binds members of a singleton component of type T.
template<typename T>
class SingletonBinder {
public:
	SingletonBinder(SaveRegistry& registry, std::string keyPrefix) :
			registry_(registry), keyPrefix_(std::move(keyPrefix)) {}

	template<typename M>
	SingletonBinder& Member(const std::string& name, M T::* member) {
		registry_.fields.push_back(
			SaveField{
				keyPrefix_.empty() ? name : keyPrefix_ + "." + name,
				// Present-only on save, ensure-on-load — mirrors the entity Member bindings so an
				// unset singleton is skipped rather than asserting in get<T>()/get_mut<T>().
				[member](const flecs::world& world) -> Json {
					return world.has<T>() ? Json(world.get<T>().*member) : Json(nullptr);
				},
				[member](const flecs::world& world, const Json& value) {
					if (!value.is_null()) {
						if (!world.has<T>()) {
							world.add<T>();
						}
						world.get_mut<T>().*member = value.get<M>();
					}
				}
			}
		);
		return *this;
	}

private:
	SaveRegistry& registry_;
	std::string keyPrefix_;
};

// Binds a single custom keyed value via Get/Set lambdas.
class FieldBinder {
public:
	FieldBinder(SaveRegistry& registry, const size_t index) : registry_(registry), index_(index) {}

	FieldBinder& Get(std::function<Json(const flecs::world&)> getter) {
		registry_.fields[index_].get = std::move(getter);
		return *this;
	}

	FieldBinder& Set(std::function<void(flecs::world&, const Json&)> setter) {
		registry_.fields[index_].set = std::move(setter);
		return *this;
	}

private:
	SaveRegistry& registry_;
	size_t index_;
};

// Binds the serialized shape of entities carrying the tag Tag.
template<typename Tag>
class EntityBinder {
public:
	EntityBinder(SaveRegistry& registry, const size_t index) : registry_(registry), index_(index) {}

	// Bind one member of component T. Present-only: the member is written only if the entity has T,
	// and on load T is added (ensure) if missing.
	template<typename T, typename M>
	EntityBinder& Member(const std::string& name, M T::* member) {
		registry_.entityTypes[index_].members.push_back(
			EntityMemberBinding{
				name,
				[name, member](const flecs::entity& entity, Json& object) {
					if (entity.has<T>()) {
						object[name] = entity.get<T>().*member;
					}
				},
				[name, member](const flecs::entity& entity, const Json& object) {
					if (const auto it = object.find(name); it != object.end()) {
						entity.ensure<T>().*member = it->get<M>();
					}
				}
			}
		);
		return *this;
	}

	// Bind a whole component T under `name` (requires nlohmann to_json/from_json for T).
	template<typename T>
	EntityBinder& Component(const std::string& name) {
		registry_.entityTypes[index_].members.push_back(
			EntityMemberBinding{
				name,
				[name](const flecs::entity& entity, Json& object) {
					if (entity.has<T>()) {
						object[name] = entity.get<T>();
					}
				},
				[name](const flecs::entity& entity, const Json& object) {
					if (const auto it = object.find(name); it != object.end()) {
						entity.ensure<T>() = it->get<T>();
					}
				}
			}
		);
		return *this;
	}

	// Escape hatch: fully control serialization. Runs after member bindings; if it returns an
	// object it is merged over the member output, otherwise it replaces it.
	EntityBinder& Serialize(std::function<Json(flecs::entity)> serializer) {
		registry_.entityTypes[index_].customSerialize = std::move(serializer);
		return *this;
	}

	// Escape hatch: fully control deserialization. Runs after member loads on the freshly created,
	// already-tagged entity.
	EntityBinder& Deserialize(std::function<void(flecs::entity, const Json&)> deserializer) {
		registry_.entityTypes[index_].customDeserialize = std::move(deserializer);
		return *this;
	}

private:
	SaveRegistry& registry_;
	size_t index_;
};

// ── SaveRegistry method definitions (builders are complete above) ───────────────────────────────

template<typename T>
SingletonBinder<T> SaveRegistry::Singleton(std::string keyPrefix) {
	return SingletonBinder<T>(*this, std::move(keyPrefix));
}

inline FieldBinder SaveRegistry::Field(std::string key) {
	fields.push_back(SaveField{std::move(key), nullptr, nullptr});
	return {*this, fields.size() - 1};
}

template<typename Tag>
EntityBinder<Tag> SaveRegistry::Entities(std::string key) {
	EntitySaveType type;
	type.key = std::move(key);
	type.collect = [](const flecs::world& world, std::vector<flecs::entity>& out) {
		world.query_builder().with<Tag>().build().each([&out](const flecs::entity& entity) { out.push_back(entity); });
	};
	type.create = [](const flecs::world& world) { return world.entity().add<Tag>(); };
	entityTypes.push_back(std::move(type));
	return EntityBinder<Tag>(*this, entityTypes.size() - 1);
}

// ── Serialization entry points ──────────────────────────────────────────────────────────────────

// Serialize the world into a JSON document using the registered schema. Reads only, so it accepts a
// const world.
[[nodiscard]] Json SaveToJson(const flecs::world& world);

// Apply a previously saved JSON document to the world. Singleton fields are written back in place;
// tag-based entity types are rebuilt (existing tagged entities destroyed, then recreated from the
// array). Missing keys are skipped, so partial / older saves load gracefully.
void LoadFromJson(flecs::world& world, const Json& document);

// Convenience string helpers — the caller owns where the string is persisted (file, localStorage,
// IDBFS, …). `indent < 0` produces compact output.
[[nodiscard]] std::string SaveToString(const flecs::world& world, int indent = 2);
void LoadFromString(flecs::world& world, std::string_view text);

} // namespace engine::save
