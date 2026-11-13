#include "cube_scene.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "game.hpp"

using namespace engine;

namespace game {

namespace {

constexpr int kRuntimeTextureSize = 8;
constexpr float kRuntimeTextureInterval = 1.0F; // seconds between refills

constexpr std::array<core::Rgba, 4> kRuntimeTexturePalette{{
	{.r = 230, .g = 80, .b = 60, .a = 255},
	{.r = 240, .g = 200, .b = 70, .a = 255},
	{.r = 70, .g = 170, .b = 110, .a = 255},
	{.r = 60, .g = 110, .b = 220, .a = 255},
}};

// RGBA8 checkerboard for the runtime albedo demo. Each step picks the next pair of palette colors and shifts the
// checker by one cell, so every refill is visibly different.
std::vector<uint8_t> FillRuntimeTexture(const int step) {
	const auto& a = kRuntimeTexturePalette[static_cast<size_t>(step) % kRuntimeTexturePalette.size()];
	const auto& b = kRuntimeTexturePalette[static_cast<size_t>(step + 1) % kRuntimeTexturePalette.size()];
	std::vector<uint8_t> rgba(static_cast<size_t>(kRuntimeTextureSize * kRuntimeTextureSize * 4));
	for (int y = 0; y < kRuntimeTextureSize; ++y) {
		for (int x = 0; x < kRuntimeTextureSize; ++x) {
			const auto& c = (x + y + step) % 2 == 0 ? a : b;
			const auto i = static_cast<size_t>((y * kRuntimeTextureSize + x) * 4);
			rgba[i + 0] = c.r;
			rgba[i + 1] = c.g;
			rgba[i + 2] = c.b;
			rgba[i + 3] = c.a;
		}
	}
	return rgba;
}

} // namespace

void CubeScene::InitializePipeline(const flecs::world& world) {
	pipeline_ = ecs::FramePipeline(world).without<scene::MenuScene>().build();
	pausedPipeline_ = ecs::FramePipeline(world).without<scene::MenuScene>().without<ecs::Pausable>().build();
	fixedStepPipeline_ = ecs::FixedStepPipeline(world).without<scene::MenuScene>().build();
}

void CubeScene::Load(flecs::world& world) {
	// Restore the debug-menu default each activation — Unload disables it, so without this it would
	// stay off after the first Game→Title→Game round-trip.
	world.get_mut<GameData>().showDebugMenu = true;

	// A single scene-root entity owns every demo entity (via child_of), so Unload can tear the
	// whole tree down with one destruct — no hand-maintained cleanup list to keep in sync.
	sceneRoot_ = world.entity("CubeSceneRoot");
	SetupPhysicsDemo(world);
	SetupRuntimeAlbedo(world);
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

// Shows a texture uploaded at runtime as the albedo of the level's lit RuntimeAlbedoQuad. An AlbedoMap with an
// empty path keeps the handle it is given, so this scene owns the texture and frees it in Unload.
void CubeScene::SetupRuntimeAlbedo(const flecs::world& world) {
	const auto quad = sceneRoot_.lookup("RuntimeAlbedoQuad");
	if (!quad) {
		spdlog::warn("[CubeScene] RuntimeAlbedoQuad missing from the demo level; runtime albedo demo skipped");
		return;
	}
	auto* platform = world.get<platform::PlatformRef>().ptr;
	runtimeTextureStep_ = 0;
	runtimeTextureTimer_ = 0.0F;
	runtimeTexture_ = platform->UploadTexture(kRuntimeTextureSize, kRuntimeTextureSize, FillRuntimeTexture(0));
	if (runtimeTexture_ < 0) {
		return;
	}
	platform->SetTextureFilter(runtimeTexture_, false); // crisp checker cells
	quad.set<render::AlbedoMap>({.path = "", .handle = runtimeTexture_});
}

// Refills the runtime albedo in place once a second; the quad's AlbedoMap handle stays the same.
void CubeScene::TickRuntimeAlbedo(const flecs::world& world, const float deltaTime) {
	if (runtimeTexture_ < 0) {
		return;
	}
	runtimeTextureTimer_ += deltaTime;
	if (runtimeTextureTimer_ < kRuntimeTextureInterval) {
		return;
	}
	runtimeTextureTimer_ -= kRuntimeTextureInterval;
	++runtimeTextureStep_;
	world.get<platform::PlatformRef>().ptr->UpdateTexture(runtimeTexture_, FillRuntimeTexture(runtimeTextureStep_));
}

void CubeScene::BuildUI(const flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());

	uiRoot_ = world.entity("GameUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});

	// Per-system Flecs timing overlay (toggle with F4).
	std::ignore =
		ui::CreateSystemStatsPanel(world, {.x = 20.0F, .y = 20.0F, .w = 420.0F, .h = h - 40.0F}).child_of(uiRoot_);

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

void CubeScene::Tick(const flecs::world& world, const float deltaTime) {
	const bool paused = world.has<scene::Paused>();
	if (!paused) {
		TickRuntimeAlbedo(world, deltaTime);
	}
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
	// The quad's AlbedoMap had no path, so destroying it released nothing: the texture is this scene's to free.
	if (runtimeTexture_ >= 0) {
		world.get<platform::PlatformRef>().ptr->UnloadTexture(runtimeTexture_);
		runtimeTexture_ = -1;
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
