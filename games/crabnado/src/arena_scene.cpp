#include "arena_scene.hpp"

#include <string>

#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

void ArenaScene::InitializePipeline(const flecs::world& world) {
	// clang-format off
	pipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::MenuScene>()
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();
	// clang-format on
}

void ArenaScene::Load(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());
	const std::string heading = "Game";

	uiRoot_ = world.entity("ArenaUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});

	std::ignore = ui::CreateLabel(world, {.x = 0.0F, .y = h / 2.0F - 140.0F, .w = w, .h = 64.0F}, heading, 64.0F)
					  .set<ui::Label>({.text = heading, .font_size = 64.0F, .color = core::colors::Title})
					  .child_of(uiRoot_);

	std::ignore = ui::CreateButton(
					  world,
					  {.x = w / 2.0F - 100.0F, .y = h / 2.0F, .w = 200.0F, .h = 56.0F},
					  "Title",
					  [onTitle = onTitlePressed](flecs::entity) {
						  if (onTitle) {
							  onTitle();
						  }
					  }
	)
					  .set_name("TitleButton")
					  .child_of(uiRoot_);
}

void ArenaScene::Unload(flecs::world&) {
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = {};
	}
}

} // namespace crabnado
