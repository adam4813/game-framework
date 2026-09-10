#include "level_module.hpp"

#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include <flecs.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "level_components.hpp"

namespace engine::level {

namespace {

using json = nlohmann::json;


// Builds entities from a JSON definition tree using the factories registered in LevelRegistry
// (Builder pattern). For each entity it assembles the components, wires the link to its parent (a
// relationship beyond the implicit ChildOf) and recurses into children — so a material can be a
// first-class nested child linked to its renderable via "render_with".
class EntityBuilder {
public:
	explicit EntityBuilder(const flecs::world& world) : world_(world) {}

	flecs::entity Build(const json& def, const flecs::entity parent) {
		// Create anonymously first, then parent, then name. The lookup-free anonymous create avoids a
		// name collision when sibling subtrees under different parents share a child name (e.g. every
		// renderable's material): under a deferred scene Load the child_of reparent is not yet applied,
		// so a named create (world.entity(name) is lookup-or-create) would resolve the shared name to
		// the same entity and silently merge them. Naming after child_of scopes the name under the
		// parent, keeping each child distinct — and gives levels "instantiate fresh" semantics.
		flecs::entity e = world_.entity();
		if (parent) {
			e.child_of(parent);
		}
		if (def.contains("name")) {
			const auto name = def.at("name").get<std::string>();
			e.set_name(name.c_str());
			named_[name] = e; // record for name-based ref resolution (see ResolveRefs)
		}
		ApplyComponents(e, def);
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
				Build(child, e);
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

	void ApplyComponents(const flecs::entity e, const json& def) const {
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
			it->second(e, value);
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

} // namespace

LevelModule::LevelModule(const flecs::world& world) {
	world.set<LevelRegistry>({});
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
	const json& doc = *doc_opt;

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
			builder.Build(def, parent);
			++count;
		}
		builder.ResolveRefs(); // second pass: wire "refs" now that all named entities exist
	}
	spdlog::info("[Level] Loaded '{}' ({} top-level entities)", path, count);
	return count;
}

} // namespace engine::level
