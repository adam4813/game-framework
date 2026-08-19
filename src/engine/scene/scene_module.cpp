#include "scene_module.hpp"

#include <vector>

#include <spdlog/spdlog.h>

#include <flecs.h>

#include "engine/ecs/ecs.hpp"
#include "engine/input/input.hpp"
#include "scene-base.hpp"
#include "scene_components.hpp"

namespace engine::scene {

SceneManagementModule::SceneManagementModule(const flecs::world& world) {
	world.component<SceneComponent>();
	// Single-active is enforced procedurally in ActivateScene.
	world.component<Active>();

	// Register mode/filter tags. Identity tags are defined and registered by each concrete scene.
	world.component<GameScene>();
	world.component<MenuScene>();

	// OnUI runs after all flecs::OnStore systems — 2D overlays always composite on top of 3D.
	world.component<ecs::OnUI>().add(flecs::Phase).add(flecs::DependsOn, flecs::OnStore);

	RegisterSceneObservers(world);
	RegisterSceneSystems(world);
}

void SceneManagementModule::RegisterSceneObservers(const flecs::world& world) {
	world.observer<SceneComponent>("OnSceneComponentAdded")
		.event(flecs::OnSet)
		.each([](const flecs::iter& it, const size_t, const SceneComponent& scene_comp) {
			if (scene_comp.instance) {
				scene_comp.instance->InitializePipeline(it.world());
			}
		});

	world.observer<Active>("OnSceneActivate")
		.event(flecs::OnAdd)
		.with<SceneComponent>()
		.each([](const flecs::iter& it, const size_t i, Active) {
			auto it_world = it.world();
			if (const auto entity = it.entity(i); entity.has<SceneComponent>()) {
				if (auto& [instance] = entity.get<SceneComponent>(); instance) {
					it_world.set_pipeline(instance->GetPipeline());
					instance->Load(it_world);
				}
			}
		});

	world.observer<Active>("OnSceneDeactivate")
		.event(flecs::OnRemove)
		.with<SceneComponent>()
		.each([](const flecs::iter& it, const size_t i, Active) {
			auto it_world = it.world();
			if (const auto entity = it.entity(i); entity.has<SceneComponent>()) {
				if (const auto& [instance] = entity.get<SceneComponent>(); instance) {
					instance->Unload(it_world);
				}
			}
		});

	// Pausing is event-driven: adding/removing the Paused tag on the world swaps the active
	// scene's pipeline. The paused pipeline excludes engine::ecs::Pausable systems, so the
	// simulation halts while input, rendering, audio and the pause menu keep running.
	const auto active_scenes = world.query_builder<const SceneComponent>().with<Active>().build();
	world.observer<Paused>("OnPauseStateChanged")
		.event(flecs::OnAdd)
		.event(flecs::OnRemove)
		.each([active_scenes](const flecs::iter& it, size_t, Paused) {
			const bool paused = it.event() == flecs::OnAdd;
			const auto it_world = it.world();
			active_scenes.each([&](const flecs::entity&, const SceneComponent& scene_comp) {
				if (scene_comp.instance) {
					it_world.set_pipeline(
						paused ? scene_comp.instance->GetPausedPipeline() : scene_comp.instance->GetPipeline()
					);
				}
			});
		});
}

void SceneManagementModule::RegisterSceneSystems(const flecs::world& world) {
	// System to tick the active scene
	world.system<SceneComponent>("ActiveSceneTick")
		.with<Active>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, const SceneComponent& scene_comp) {
			if (scene_comp.instance) {
				scene_comp.instance->Tick(it.world(), it.delta_time());
			}
		});

	// System to handle input for the active scene
	world.system<SceneComponent, const input::InputState>("ActiveSceneInput")
		.term_at<input::InputState>()
		.singleton()
		.with<Active>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, const SceneComponent& scene_comp, const input::InputState& input) {
			if (scene_comp.instance) {
				scene_comp.instance->HandleInput(it.world(), input);
			}
		});

	// System to draw debug UI for the active scene
	world.system<const SceneComponent>("ActiveSceneDebugUI")
		.with<Active>()
		.kind<ecs::OnUI>()
		.each([](const flecs::iter&, size_t, const SceneComponent& scene_comp) {
			if (scene_comp.instance) {
				scene_comp.instance->DrawDebugUI();
			}
		});

	// System to draw 2D scene UI (overlays, HUD, buttons) — runs in OnUI (after OnStore) so
	// 2D elements always composite on top of 3D geometry regardless of module import order.
	world.system<SceneComponent, const input::InputState>("ActiveSceneUI")
		.term_at<input::InputState>()
		.singleton()
		.with<Active>()
		.kind<ecs::OnUI>()
		.each([](const flecs::iter& it, size_t, const SceneComponent& scene_comp, const input::InputState& input) {
			if (scene_comp.instance) {
				scene_comp.instance->DrawUI(it.world(), input);
			}
		});
}

} // namespace engine::scene
