#include "level_module.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include <flecs.h>
#include <nlohmann/json.hpp>

#include "engine/core/core.hpp"
#include "level_components.hpp"

namespace engine::level {

namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

// Sigil prefix for file references inside JSON level files.
// Use "@file:path/to/file.json" to load JSON from a file and substitute it in place.
// Valid anywhere a JSON value or entity definition is expected:
//   - Entity/child arrays:    "entities": [ "@file:npcs/player.json", { ... } ]
//   - Component values:       "component_name": "@file:data/material.json"
// Bare strings in arrays are reserved for future entity-by-name references and are NOT treated
// as file refs; the sigil is always required to trigger file loading.
constexpr std::string_view kFileRefSigil = "@file:";

// Strip the file-ref sigil and resolve the path relative to `base_dir`. Returns a null optional
// and logs on failure (file not found, parse error, etc.).
[[nodiscard]] std::optional<json> LoadFileRef(const std::string_view ref, const fs::path& base_dir) {
	const auto path = ref.substr(kFileRefSigil.size());
	const auto full = (base_dir / path).lexically_normal();
	return core::LoadJsonFile(full.string(), "Level:FileRef");
}

[[nodiscard]] bool IsFileRef(const json& val) {
	return val.is_string() && val.get_ref<const std::string&>().starts_with(kFileRefSigil);
}

// Builds entities from a JSON definition tree using the factories registered in LevelRegistry
// (Builder pattern). For each entity it assembles the components, wires the link to its parent (a
// relationship beyond the implicit ChildOf) and recurses into children — so a material can be a
// first-class nested child linked to its renderable via "render_with".
//
// File references are resolved relative to `cur_dir` at each call site. When an entity is loaded
// from a file, its own children resolve relative to *that* file's directory, so entity files are
// self-contained and portable.
class EntityBuilder {
public:
	explicit EntityBuilder(const flecs::world& world) : world_(world) {}

	// Build an entity from `def` under `parent`. `cur_dir` is used to resolve any "@file:" references.
	// If `def` is an "@file:" string, the referenced file is loaded and built in its own directory.
	flecs::entity Build(const json& def, const flecs::entity parent, const fs::path& cur_dir) {
		if (IsFileRef(def)) {
			return BuildFromFile(def.get<std::string>(), parent, cur_dir);
		}

		if (!def.is_object()) {
			spdlog::warn("[Level] Entity definition must be an object or \"@file:\" reference — skipping");
			return {};
		}

		// Create anonymously first, then parent, then name. The lookup-free anonymous create avoids a
		// name collision when sibling subtrees under different parents share a child name (e.g. every
		// renderable's material): under a deferred scene Load the child_of reparent is not yet applied,
		// so a named create (world.entity(name) is lookup-or-create) would resolve the shared name to
		// the same entity and silently merge them. Naming after child_of scopes the name under the
		// parent, keeping each child distinct — and gives levels "instantiate fresh" semantics.
		const flecs::entity e = world_.entity();
		if (parent) {
			e.child_of(parent);
		}
		if (def.contains("name")) {
			const auto name = def.at("name").get<std::string>();
			e.set_name(name.c_str());
			named_[name] = e; // record for name-based ref resolution (see ResolveRefs)
		}
		ApplyComponents(e, def, cur_dir);
		if (parent) {
			ApplyLink(parent, e, def);
		}
		if (def.contains("refs")) {
			for (const auto& [ref, target] : def.at("refs").items()) {
				pending_refs_.push_back({.self = e, .ref = ref, .target = target.get<std::string>()});
			}
		}
		if (def.contains("children")) {
			for (const auto& child : def.at("children")) {
				Build(child, e, cur_dir);
			}
		}
		return e;
	}

	// Second pass: wire every "refs" entry now that all named entities exist. Runs after the whole
	// tree is built so an entity can reference a sibling declared later in the file.
	void ResolveRefs() {
		const auto& reg = world_.get<LevelRegistry>();
		for (const auto& [self, ref, target] : pending_refs_) {
			const auto ref_it = reg.refLoaders.find(ref);
			if (ref_it == reg.refLoaders.end()) {
				spdlog::warn("[Level] Unknown ref '{}' on entity '{}'", ref, self.name().c_str());
				continue;
			}
			const auto target_it = named_.find(target);
			if (target_it == named_.end()) {
				spdlog::warn("[Level] Ref '{}' targets unknown entity '{}'", ref, target);
				continue;
			}
			ref_it->second(self, target_it->second);
		}
	}

private:
	struct PendingRef {
		flecs::entity self;
		std::string ref;
		std::string target;
	};

	// Load an entity definition from a "@file:" reference and build it under `parent`.
	// The loaded file's own directory becomes the base for any nested "@file:" refs inside it.
	flecs::entity BuildFromFile(const std::string& ref, const flecs::entity parent, const fs::path& cur_dir) {
		const auto doc_opt = LoadFileRef(ref, cur_dir);
		if (!doc_opt) {
			return {};
		}
		const auto rel_path = ref.substr(kFileRefSigil.size());
		const auto file_dir = (cur_dir / rel_path).lexically_normal().parent_path();
		return Build(*doc_opt, parent, file_dir);
	}

	void ApplyComponents(const flecs::entity e, const json& def, const fs::path& cur_dir) const {
		if (!def.contains("components")) {
			return;
		}
		const auto& reg = world_.get<LevelRegistry>();
		for (const auto& [key, value] : def.at("components").items()) {
			const auto it = reg.loaders.find(key);
			if (it == reg.loaders.end()) {
				spdlog::warn("[Level] Unknown component '{}' on entity '{}'", key, e.name().c_str());
				continue;
			}
			// A "@file:" string component value loads the JSON from disk and substitutes it.
			if (IsFileRef(value)) {
				const auto file_data = LoadFileRef(value.get<std::string>(), cur_dir);
				if (!file_data) {
					continue;
				}
				ApplyComponent(it->second, e, key, *file_data);
				continue;
			}
			ApplyComponent(it->second, e, key, value);
		}
	}

	// A loader reading a wrongly typed value (nlohmann throws on `get<int>()` of a string) skips that
	// one component with a warning, rather than aborting the whole level load.
	static void
	ApplyComponent(const ComponentLoader& loader, const flecs::entity e, const std::string& key, const json& value) {
		try {
			loader(e, value);
		}
		catch (const json::exception& ex) {
			spdlog::warn("[Level] Skipping component '{}' on entity '{}': {}", key, e.name().c_str(), ex.what());
		}
	}

	void ApplyLink(const flecs::entity parent, const flecs::entity child, const json& def) const {
		if (!def.contains("link")) {
			return;
		}
		const auto link = def.at("link").get<std::string>();
		const auto& reg = world_.get<LevelRegistry>();
		const auto it = reg.linkLoaders.find(link);
		if (it == reg.linkLoaders.end()) {
			spdlog::warn("[Level] Unknown link '{}' on entity '{}'", link, child.name().c_str());
			return;
		}
		it->second(parent, child);
	}

	const flecs::world& world_;
	std::unordered_map<std::string, flecs::entity> named_;
	std::vector<PendingRef> pending_refs_;
};

int LoadLevelDocument(
	const flecs::world& world,
	const json& doc,
	const flecs::entity parent,
	const fs::path& base_dir
) {
	if (doc.contains("singletons")) {
		const auto& reg = world.get<LevelRegistry>();
		for (const auto& [key, value] : doc.at("singletons").items()) {
			const auto it = reg.singletonLoaders.find(key);
			if (it == reg.singletonLoaders.end()) {
				spdlog::warn("[Level] Unknown singleton '{}'", key);
				continue;
			}
			it->second(world, value);
		}
	}

	int count = 0;
	if (doc.contains("entities")) {
		EntityBuilder builder(world);
		for (const auto& def : doc.at("entities")) {
			builder.Build(def, parent, base_dir);
			++count;
		}
		builder.ResolveRefs();
	}

	return count;
}

} // namespace

LevelModule::LevelModule(const flecs::world& world) {
	world.set<LevelRegistry>({});

	// UseRegistry relationship: pairs a consumer entity with a registry-owning provider.
	// JSON: "refs": { "use_registry": "ProviderEntityName" }
	world.component<UseRegistry>().add(flecs::Traversable);
	RegisterRef(world, "use_registry", [](const flecs::entity self, const flecs::entity target) {
		self.add<UseRegistry>(target);
	});

	spdlog::info("[LevelModule] Registered level registry (module factories populate it on import)");
}

void RegisterComponentLoader(const flecs::world& world, const std::string_view name, ComponentLoader loader) {
	if (!world.has<LevelRegistry>()) {
		return;
	}
	world.get_mut<LevelRegistry>().loaders[std::string{name}] = std::move(loader);
}

void RegisterSingletonLoader(const flecs::world& world, const std::string_view name, SingletonLoader loader) {
	if (!world.has<LevelRegistry>()) {
		return;
	}
	world.get_mut<LevelRegistry>().singletonLoaders[std::string{name}] = std::move(loader);
}

void RegisterLink(const flecs::world& world, const std::string_view name, LinkLoader loader) {
	if (!world.has<LevelRegistry>()) {
		return;
	}
	world.get_mut<LevelRegistry>().linkLoaders[std::string{name}] = std::move(loader);
}

void RegisterRef(const flecs::world& world, const std::string_view name, RefLoader loader) {
	if (!world.has<LevelRegistry>()) {
		return;
	}
	world.get_mut<LevelRegistry>().refLoaders[std::string{name}] = std::move(loader);
}

int LoadLevel(const flecs::world& world, const std::string_view path, const flecs::entity parent) {
	if (!world.has<LevelRegistry>()) {
		spdlog::error("[Level] LevelRegistry not set — import LevelModule before loading a level");
		return -1;
	}

	const auto doc_opt = core::LoadJsonFile(path, "Level");
	if (!doc_opt) {
		return -1;
	}
	const auto base_dir = fs::path{path}.lexically_normal().parent_path();
	const int count = LoadLevelDocument(world, *doc_opt, parent, base_dir);
	spdlog::info("[Level] Loaded '{}' ({} top-level entities)", path, count);
	return count;
}

int LoadLevelFromSource(const flecs::world& world, const std::string_view json_source, const flecs::entity parent) {
	if (!world.has<LevelRegistry>()) {
		spdlog::error("[Level] LevelRegistry not set — import LevelModule before loading a level");
		return -1;
	}

	json doc;
	try {
		doc = json::parse(json_source);
	}
	catch (const json::parse_error& e) {
		spdlog::error("[Level] JSON parse error: {}", e.what());
		return -1;
	}

	const int count = LoadLevelDocument(world, doc, parent, fs::current_path());
	spdlog::info("[Level] Loaded inline level source ({} top-level entities)", count);
	return count;
}

} // namespace engine::level
