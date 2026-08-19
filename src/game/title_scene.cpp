#include "title_scene.hpp"

#include <string>

#include "constants.hpp"
#include "engine/engine.hpp"
#include "tilemap_scene.hpp"

using namespace engine;

namespace game {

void TitleScene::InitializePipeline(const flecs::world& world) {
	// clang-format off
	pipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::GameScene>()
					// Re-inject the built-in phase sorting logic:
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
	const std::string assetDir = platform->AssetDirectory();

	// Root container spanning the screen; children are positioned in absolute screen coordinates.
	uiRoot_ = world.entity("TitleUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});

	// Title + hint text.
	std::ignore = ui::CreateLabel(
					  world,
					  {.x = 0.0F, .y = h / 2.0F - 200.0F, .w = w, .h = 64.0F},
					  std::string(app::constants::kWindowTitle),
					  64.0F
	)
					  .set<ui::Label>(
						  {.text = std::string(app::constants::kWindowTitle),
						   .font_size = 64.0F,
						   .color = platform::colors::Title}
					  )
					  .child_of(uiRoot_);

	std::ignore = ui::CreateLabel(
					  world,
					  {.x = 0.0F, .y = h / 2.0F + 70.0F, .w = w, .h = 20.0F},
					  "Press F1 for the debug menu",
					  16.0F
	)
					  .set<ui::Label>(
						  {.text = "Press F1 for the debug menu", .font_size = 16.0F, .color = platform::colors::Subtle}
					  )
					  .child_of(uiRoot_);

	// Play Cube button (left)
	std::ignore = ui::CreateButton(
					  world,
					  {.x = w / 2.0F - 230.0F, .y = h / 2.0F - 20.0F, .w = 200.0F, .h = 56.0F},
					  "Play Cube",
					  [onPlay = onPlayPressed](flecs::entity) {
						  if (onPlay) {
							  onPlay();
						  }
					  }
	)
					  .set_name("PlayCubeButton")
					  .child_of(uiRoot_);

	// Play Tilemap button (right)
	std::ignore = ui::CreateButton(
					  world,
					  {.x = w / 2.0F + 30.0F, .y = h / 2.0F - 20.0F, .w = 200.0F, .h = 56.0F},
					  "Play Tilemap",
					  [](const flecs::entity e) { scene::ActivateScene<TilemapSceneTag>(e.world()); }
	)
					  .set_name("PlayTilemapButton")
					  .child_of(uiRoot_);

	// Script-bound button: it is created with no C++ handler; a script assigns its onClick via
	// SetOnClick (see assets/scripts/button_click_demo.as), demonstrating callback binding to a
	// flecs component's existing storage.
	const auto scriptButton = ui::CreateButton(
								  world,
								  {.x = w / 2.0F - 100.0F, .y = h / 2.0F + 52.0F, .w = 200.0F, .h = 48.0F},
								  "Script Button",
								  nullptr
	)
								  .set_name("ScriptButton")
								  .child_of(uiRoot_);
	world.entity("ButtonClickDemoScript")
		.child_of(scriptButton)
		.set<scripting::ScriptComponent>({.source_path = assetDir + "/scripts/button_click_demo.as"});

	// === Widget showcase panel (progress bar, spinner, scrollable list) ===
	const auto panel = ui::CreatePanel(
						   world,
						   {.x = w - 340.0F, .y = h / 2.0F - 175.0F, .w = 300.0F, .h = 350.0F},
						   {.color = platform::colors::PanelBg, .roundness = 0.06F}
	)
						   .child_of(uiRoot_);

	std::ignore =
		ui::CreateLabel(world, {.x = w - 340.0F, .y = h / 2.0F - 168.0F, .w = 300.0F, .h = 28.0F}, "UI Widgets", 22.0F)
			.set<ui::Label>({.text = "UI Widgets", .font_size = 22.0F, .color = platform::colors::Title})
			.child_of(panel);

	// Progress bar driven live by a script (see assets/scripts/ui_demo.as).
	const auto bar =
		ui::CreateProgressBar(world, {.x = w - 320.0F, .y = h / 2.0F - 125.0F, .w = 260.0F, .h = 26.0F}, {})
			.set_name("DemoProgress")
			.child_of(panel);
	world.entity("UIDemoScript")
		.child_of(bar)
		.set<scripting::ScriptComponent>({.source_path = assetDir + "/scripts/ui_demo.as"});

	// Loading spinner + label.
	std::ignore =
		ui::CreateSpinner(world, {.x = w - 320.0F, .y = h / 2.0F - 92.0F, .w = 56.0F, .h = 56.0F}, {}).child_of(panel);
	const auto loading =
		ui::CreateLabel(world, {.x = w - 256.0F, .y = h / 2.0F - 92.0F, .w = 190.0F, .h = 56.0F}, "Loading...", 18.0F)
			.set<ui::Label>(
				{.text = "Loading...",
				 .font_size = 18.0F,
				 .color = platform::colors::Text,
				 .align = ui::TextAlign::Left}
			)
			.child_of(panel);

	// Scrollable list: a ScrollRect clipping a vertical Stack of item buttons. Clicking an item
	// updates the loading label — a button-updates-another-component interaction.
	const auto scroll =
		ui::CreateScrollRect(world, {.x = w - 320.0F, .y = h / 2.0F - 15.0F, .w = 260.0F, .h = 175.0F}, {})
			.child_of(panel);
	const auto list = ui::CreateStack(
						  world,
						  {.x = w - 320.0F, .y = h / 2.0F - 15.0F, .w = 260.0F, .h = 0.0F},
						  {.direction = ui::StackDirection::Vertical, .spacing = 6.0F, .padding = 6.0F}
	)
						  .child_of(scroll);
	for (int i = 1; i <= 10; ++i) {
		const std::string label = "Item " + std::to_string(i);
		std::ignore =
			ui::CreateButton(world, {.x = 0.0F, .y = 0.0F, .w = 0.0F, .h = 34.0F}, label, [loading, i](flecs::entity) {
				loading.get_mut<ui::Label>().text = "Picked item " + std::to_string(i);
			}).child_of(list);
	}
}

void TitleScene::Unload(flecs::world&) {
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = flecs::entity{};
	}
}

} // namespace game
