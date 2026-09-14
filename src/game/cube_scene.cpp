#include "cube_scene.hpp"

#include <string>

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "game.hpp"

using namespace engine;

namespace game {

void CubeScene::InitializePipeline(const flecs::world& world) {
	// clang-format off
	pipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::MenuScene>()
					// Re-inject the built-in phase sorting logic:
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();

	pausedPipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::MenuScene>()
					.without<ecs::Pausable>()
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();
	// clang-format on
}

void CubeScene::Load(flecs::world& world) {
	// Restore the debug-menu default each activation — Unload disables it, so without this it would
	// stay off after the first Game→Title→Game round-trip.
	world.get_mut<GameData>().showDebugMenu = true;

	// A single scene-root entity owns every demo entity (via child_of), so Unload can tear the
	// whole tree down with one destruct — no hand-maintained cleanup list to keep in sync.
	sceneRoot_ = world.entity("CubeSceneRoot");
	SetupPhysicsDemo(world);
	BuildUI(world);
}

void CubeScene::SetupPhysicsDemo(const flecs::world& world) const {
	// The 3D physics demo is authored declaratively in assets/levels/demo.level.json: camera, a
	// sun light with its hue-cycle script, the floor, the falling cube (with its jump/spin/particle
	// scripts, particle emitter and jump sound), a timer-tween sphere and the save-system demo
	// script. Loading it under sceneRoot_ keeps the whole tree owned by one entity, so Unload tears
	// it down with a single destruct — the same ownership the imperative setup had, now data-driven.
	level::LoadLevel(world, assets::ResolveAsset(world, "levels/demo.level.json"), sceneRoot_);
}

void CubeScene::BuildUI(const flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());

	uiRoot_ = world.entity("GameUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});

	// HUD container — toggled visible/hidden by Tick based on pause state.
	hud_ = world.entity("GameHUD")
			   .set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}})
			   .set<ui::UIElement>({.z_index = 0, .visible = true})
			   .child_of(uiRoot_);

	std::ignore = ui::CreateLabel(world, {.x = 0.0F, .y = h / 2.0F - 100.0F, .w = w, .h = 48.0F}, "Game Scene", 48.0F)
					  .set<ui::Label>({.text = "Game Scene", .font_size = 48.0F, .color = core::colors::Title})
					  .child_of(hud_);

	std::ignore = ui::CreateLabel(world, {.x = 0.0F, .y = h / 2.0F, .w = w, .h = 20.0F}, "Press ESC to pause", 16.0F)
					  .set<ui::Label>(
						  {.text = "ESC: pause  |  Space/Click: jump  |  P: toggle particles  |  T: texture",
						   .font_size = 16.0F,
						   .color = core::colors::Subtle}
					  )
					  .child_of(hud_);

	// Pause menu, composed as a prompt (Modal + Stack + buttons). Starts closed; Tick opens it
	// while the scene is paused.
	pauseModal_ =
		ui::CreatePrompt(
			world,
			{.x = w / 2.0F - 200.0F, .y = h / 2.0F - 130.0F, .w = 400.0F, .h = 240.0F},
			"Paused",
			"",
			{
				{.label = "Resume", .on_click = [](const flecs::entity e) { e.world().remove<scene::Paused>(); }},
				{.label = "Quit to Title",
				 .on_click =
					 [onQuit = onQuitPressed](flecs::entity) {
						 if (onQuit) {
							 onQuit();
						 }
					 }},
			}
		)
			.set<ui::Modal>({.open = false})
			.child_of(uiRoot_);
}

void CubeScene::Tick(const flecs::world& world, float /*deltaTime*/) {
	const bool paused = world.has<scene::Paused>();
	if (hud_ && hud_.has<ui::UIElement>()) {
		hud_.get_mut<ui::UIElement>().visible = !paused;
	}
	if (pauseModal_ && pauseModal_.has<ui::Modal>()) {
		pauseModal_.get_mut<ui::Modal>().open = paused;
	}
}

void CubeScene::HandleInput(const flecs::world& world, const input::InputState& input) {
	// Toggle pause on Escape — input detection only; the pause menu is retained UI driven by Tick.
	if (input.keys[input::KeyCode::Escape].pressed) {
		if (world.has<scene::Paused>()) {
			world.remove<scene::Paused>();
		}
		else {
			world.add<scene::Paused>();
		}
	}
}

void CubeScene::Unload(flecs::world& world) {
	// One destruct tears down the whole demo tree (all entities are children of sceneRoot_).
	if (sceneRoot_) {
		sceneRoot_.destruct();
		sceneRoot_ = flecs::entity{};
	}
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = flecs::entity{};
	}
	// hud_/pauseModal_ were children of uiRoot_ and are already destroyed; clear the stale handles.
	hud_ = flecs::entity{};
	pauseModal_ = flecs::entity{};

	// Clear pause state and reset game data when unloading the scene.
	world.remove<scene::Paused>();
	if (world.has<render::AmbientLight>()) {
		world.remove<render::AmbientLight>();
	}
	world.get_mut<GameData>().showDebugMenu = false;
}

} // namespace game
