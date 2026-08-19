#include "save_registry.hpp"

#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace engine::save {
namespace {

// Write `value` into `root` at a dotted path, creating intermediate objects as needed.
void SetByPath(Json& root, const std::string& dottedKey, Json value) {
	Json* cursor = &root;
	size_t start = 0;
	while (true) {
		const size_t dot = dottedKey.find('.', start);
		const std::string segment = dottedKey.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
		if (dot == std::string::npos) {
			(*cursor)[segment] = std::move(value);
			return;
		}
		cursor = &(*cursor)[segment];
		start = dot + 1;
	}
}

// Read the value at a dotted path. Returns false (leaving `out` untouched) if any segment is
// missing, so absent keys are simply skipped by the caller.
bool TryGetByPath(const Json& root, const std::string& dottedKey, Json& out) {
	const Json* cursor = &root;
	size_t start = 0;
	while (true) {
		const size_t dot = dottedKey.find('.', start);
		const std::string segment = dottedKey.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
		if (!cursor->is_object()) {
			return false;
		}
		const auto it = cursor->find(segment);
		if (it == cursor->end()) {
			return false;
		}
		if (dot == std::string::npos) {
			out = *it;
			return true;
		}
		cursor = &*it;
		start = dot + 1;
	}
}

// Serialize a single tagged entity: member bindings first, then the optional custom serializer
// (merged over members when it returns an object, otherwise replacing them).
Json SerializeEntity(const EntitySaveType& type, const flecs::entity& entity) {
	Json object = Json::object();
	for (const auto& member : type.members) {
		member.save(entity, object);
	}
	if (type.customSerialize) {
		Json custom = type.customSerialize(entity);
		if (custom.is_object()) {
			object.merge_patch(custom);
		}
		else {
			object = std::move(custom);
		}
	}
	return object;
}

// Apply a JSON entry to a freshly created, already-tagged entity.
void DeserializeEntity(const EntitySaveType& type, const flecs::entity& entity, const Json& entry) {
	for (const auto& member : type.members) {
		member.load(entity, entry);
	}
	if (type.customDeserialize) {
		type.customDeserialize(entity, entry);
	}
}

} // namespace

Json SaveToJson(const flecs::world& world) {
	if (!world.has<SaveRegistry>()) {
		return Json::object();
	}
	const auto& registry = world.get<SaveRegistry>();

	Json document = Json::object();
	document["version"] = registry.version;

	for (const auto& field : registry.fields) {
		if (field.get) {
			SetByPath(document, field.key, field.get(world));
		}
	}

	for (const auto& type : registry.entityTypes) {
		std::vector<flecs::entity> entities;
		type.collect(world, entities);

		Json array = Json::array();
		for (const flecs::entity entity : entities) {
			array.push_back(SerializeEntity(type, entity));
		}
		document[type.key] = std::move(array);
	}

	return document;
}

void LoadFromJson(flecs::world& world, const Json& document) {
	if (!world.has<SaveRegistry>()) {
		return;
	}
	const auto& registry = world.get<SaveRegistry>();

	for (const auto& field : registry.fields) {
		if (!field.set) {
			continue;
		}
		if (Json value; TryGetByPath(document, field.key, value)) {
			// A single type-mismatched key (hand-edited or version-skewed save) must not abort the
			// whole load — skip it and keep going, honouring the "loads gracefully" contract.
			try {
				field.set(world, value);
			}
			catch (const Json::exception& ex) {
				spdlog::warn("[SaveModule] Skipping field '{}': {}", field.key, ex.what());
			}
		}
	}

	for (const auto& type : registry.entityTypes) {
		// Entities are ephemeral: destroy the current generation before rebuilding. Collect first so
		// we don't mutate a query while iterating it.
		std::vector<flecs::entity> existing;
		type.collect(world, existing);
		for (const flecs::entity entity : existing) {
			entity.destruct();
		}

		const auto it = document.find(type.key);
		if (it == document.end() || !it->is_array()) {
			continue;
		}
		for (const auto& entry : *it) {
			flecs::entity entity = type.create(world);
			try {
				DeserializeEntity(type, entity, entry);
			}
			catch (const Json::exception& ex) {
				spdlog::warn("[SaveModule] Skipping malformed '{}' entry: {}", type.key, ex.what());
			}
		}
	}
}

std::string SaveToString(const flecs::world& world, const int indent) { return SaveToJson(world).dump(indent); }

void LoadFromString(flecs::world& world, const std::string_view text) {
	const Json document = Json::parse(text, nullptr, /*allow_exceptions=*/false);
	if (document.is_discarded()) {
		spdlog::error("[SaveModule] LoadFromString: failed to parse save data");
		return;
	}
	LoadFromJson(world, document);
}

} // namespace engine::save
