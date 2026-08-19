#pragma once

#include <memory>
#include <vector>

#include <flecs.h>
#include <spdlog/spdlog.h>

#include "scene_components.hpp"

namespace engine::scene {

// Flecs module: Registers scene management systems and hooks into the world.
// Creates scene entities and registers lifecycle/switching systems using hooks.
class SceneManagementModule {
public:
	explicit SceneManagementModule(const flecs::world& world);

private:
	void RegisterSceneObservers(const flecs::world& world);
	void RegisterSceneSystems(const flecs::world& world);
};

// Create a scene entity: an entity tagged with SceneTag (e.g., CubeScene, TitleScene) that carries
// the scene instance (SceneComponent). The tag lets a scene be found/switched by type — including
// over the Flecs Remote API by name — without depending on runtime-assigned entity ids. Returns
// the created entity.
//
// Example:
//   auto scene = RegisterScene<CubeScene>(world, std::make_shared<CubeScene>());
//
template<typename SceneTag>
flecs::entity RegisterScene(const flecs::world& world, std::shared_ptr<SceneBase> scene) {
	// Create entity with the tag and scene component. The tag serves as both a filter and identity.
	return world.entity().add<SceneTag>().template set<SceneComponent>({std::move(scene)});
}

// Find the registered scene entity by tag type. Invalid entity if none is registered.
//
// Example:
//   auto entity = FindScene<CubeScene>(world);
//
template<typename SceneTag>
[[nodiscard]] flecs::entity FindScene(const flecs::world& world) {
	flecs::entity result;
	world.query_builder().with<SceneTag>().build().each([&result](const flecs::entity& e) {
		if (!result) {
			result = e;
		}
	});
	return result;
}

// Make the scene with the given tag type active: deactivate whatever scene is currently active,
// then activate the target (its Active tag drives Load/Unload + pipeline selection via observers).
// Safe to call from within a system — it uses deferred structural changes.
//
// Example:
//   ActivateScene<CubeScene>(world);
//
template<typename SceneTag>
void ActivateScene(const flecs::world& world) {
	const flecs::entity target = FindScene<SceneTag>(world);
	if (!target) {
		spdlog::error("[SceneManagement] ActivateScene: no scene registered for tag");
		return;
	}
	if (target.has<Active>()) {
		return;
	}

	std::vector<flecs::entity> active;
	world.query_builder().with<Active>().with<SceneComponent>().build().each([&active](const flecs::entity& e) {
		active.push_back(e);
	});
	for (const flecs::entity e : active) {
		e.remove<Active>();
	}
	target.add<Active>();
}

// Reload the scene with the given tag type in-place: Unload → Load without switching scenes.
// No-op if the scene is not currently active. Safe to call from within a system (deferred).
// Use this when you need to reset all scene entities (e.g. after applying a save snapshot) while
// staying in the same scene.
//
// Example:
//   ReloadScene<CubeScene>(world);
//
template<typename SceneTag>
void ReloadScene(const flecs::world& world) {
	const flecs::entity target = FindScene<SceneTag>(world);
	if (!target || !target.has<Active>()) {
		return; // not active — nothing to reload
	}
	spdlog::info("[SceneManagement] ReloadScene: reloading scene");
	// Remove then re-add Active in order. When called from inside a system the two commands are
	// deferred and applied at the next merge: the OnRemove observer fires first (Unload), then the
	// OnAdd observer fires (Load), giving a clean tear-down + rebuild of all scene entities.
	target.remove<Active>();
	target.add<Active>();
}

} // namespace engine::scene
