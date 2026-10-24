#include "title_scene.hpp"

#include <string>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

void TitleScene::InitializePipeline(const flecs::world& world) {
	// clang-format off
	pipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::GameScene>()
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();
	// clang-format on
}

void TitleScene::Load(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());
	const std::string title(constants::kWindowTitle);

	uiRoot_ = world.entity("TitleUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});
	std::ignore = world.entity("Music").child_of(uiRoot_).set<audio::Music>(
		{.path = assets::ResolveAsset(world, "audio/crabnado/title.ogg"), .loop = true, .volume = constants::kTitleMusicVolume}
	);

	std::ignore = ui::CreateLabel(world, {.x = 0.0F, .y = h / 2.0F - 140.0F, .w = w, .h = 64.0F}, title, 64.0F)
					  .set<ui::Label>({.text = title, .font_size = 64.0F, .color = core::colors::Title})
					  .child_of(uiRoot_);

	std::ignore = ui::CreateButton(
					  world,
					  {.x = w / 2.0F - 100.0F, .y = h / 2.0F, .w = 200.0F, .h = 56.0F},
					  "Play",
					  [onPlay = onPlayPressed](flecs::entity) {
						  if (onPlay) {
							  onPlay();
						  }
					  }
	)
					  .set_name("PlayButton")
					  .child_of(uiRoot_);
}

void TitleScene::Unload(flecs::world&) {
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = {};
	}
}

} // namespace crabnado
