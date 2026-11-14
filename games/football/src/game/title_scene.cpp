#include "title_scene.hpp"

#include <string>

#include <spdlog/fmt/fmt.h>

#include "constants.hpp"
#include "engine/engine.hpp"
#include "football/football.hpp"

using namespace engine;

namespace game {

void TitleScene::InitializePipeline(const flecs::world& world) {
	pipeline_ = ecs::FramePipeline(world).without<scene::GameScene>().build();
	fixedStepPipeline_ = ecs::FixedStepPipeline(world).without<scene::GameScene>().build();
}

void TitleScene::Load(flecs::world& world) {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());
	const int texMenu = platform->LoadTexture(assets::ResolveAsset(world, "textures/menu_bg.png"));
	const int texButton = platform->LoadTexture(assets::ResolveAsset(world, "textures/button.png"));

	// Root container spanning the screen; children are positioned in absolute screen coordinates.
	uiRoot_ = world.entity("TitleUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});
	std::ignore = ui::CreatePanel(
					  world,
					  {.x = 0.0F, .y = 0.0F, .w = w, .h = h},
					  {.color = {255, 255, 255, 255}, .border_thickness = 0.0F, .texture = texMenu}
	)
					  .child_of(uiRoot_);

	const std::string title(app::constants::kWindowTitle);
	std::ignore = ui::CreateLabel(world, {.x = 0.0F, .y = h / 2.0F - 180.0F, .w = w, .h = 72.0F}, title, 72.0F)
					  .set<ui::Label>({.text = title, .font_size = 72.0F, .color = core::colors::Title})
					  .child_of(uiRoot_);

	const std::string best = fmt::format("High Score: {}", world.get<football::HighScore>().best_points);
	std::ignore = ui::CreateLabel(world, {.x = 0.0F, .y = h / 2.0F - 80.0F, .w = w, .h = 36.0F}, best, 32.0F)
					  .set<ui::Label>({.text = best, .font_size = 32.0F, .color = core::colors::Boost})
					  .child_of(uiRoot_);

	std::ignore = ui::CreateButton(
					  world,
					  {.x = w / 2.0F - 120.0F, .y = h / 2.0F + 10.0F, .w = 240.0F, .h = 60.0F},
					  "Play",
					  [](const flecs::entity e) { scene::ActivateScene<football::FootballSceneTag>(e.world()); },
					  texButton
	)
					  .set_name("PlayButton")
					  .child_of(uiRoot_);
}

void TitleScene::Unload(flecs::world&) {
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = flecs::entity{};
	}
}

} // namespace game
