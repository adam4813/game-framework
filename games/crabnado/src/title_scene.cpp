#include "title_scene.hpp"

#include <array>
#include <string>
#include <string_view>

#include "crabnado_constants.hpp"
#include "settings.hpp"
#include "settings_menu.hpp"
#include "title_backdrop.hpp"
#include "title_logo.hpp"
#include "ui_theme.hpp"
#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr float kCreditsWidth = 900.0F;
constexpr float kCreditsHeight = 580.0F;
constexpr float kCreditsLineHeight = 52.0F;
constexpr float kTitleFontSize = 96.0F;
constexpr float kTitleTop = 0.07F; // fraction of the screen height, leaving room for the shells perched on top
constexpr core::Rgba kTitleColor{.r = 255, .g = 244, .b = 220, .a = 255};
constexpr core::Rgba kTitleOutlineColor{.r = 18, .g = 40, .b = 70, .a = 255};
constexpr float kTitleOutlineWidth = 5.0F;
constexpr float kButtonWidth = 200.0F;
constexpr float kButtonHeight = 56.0F;
constexpr float kButtonGap = 24.0F;
// Settings is a size down from Play and Credits, centred under them.
constexpr float kSmallButtonWidth = 160.0F;
constexpr float kSmallButtonHeight = 40.0F;
constexpr float kSmallButtonGap = 14.0F;
constexpr float kSmallButtonFontSize = 22.0F;
constexpr float kSmallButtonBorder = 3.0F;

} // namespace

void TitleScene::InitializePipeline(const flecs::world& world) {
	// The backdrop is 3D, and the engine's render systems are tagged GameScene, so this pipeline admits them.
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

void TitleScene::Load(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());
	const std::string title(constants::kWindowTitle);

	backdrop_ = world.entity("TitleBackdrop");
	BuildTitleBackdrop(world, backdrop_);

	uiRoot_ = world.entity("TitleUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});
	std::ignore = world.entity("Music")
					  .child_of(uiRoot_)
					  .set<audio::Music>({.path = assets::ResolveAsset(world, "audio/crabnado/title.ogg"), .loop = true})
					  .set<MusicLevel>({constants::kTitleMusicVolume});

	std::ignore = ui::CreateLabel(world, {.x = 0.0F, .y = h * kTitleTop, .w = w, .h = kTitleFontSize + 8.0F}, title, kTitleFontSize)
					  .set<ui::Label>({.text = title,
									   .font_size = kTitleFontSize,
									   .color = kTitleColor,
									   .outline = kTitleOutlineWidth,
									   .outline_color = kTitleOutlineColor})
					  .child_of(uiRoot_);
	// The label centres its text vertically, so the glyphs start half the slack below the rect's top.
	std::ignore = world.entity("TitleLogo").child_of(uiRoot_).set<TitleLogo>({.top = h * kTitleTop + 4.0F, .font_size = kTitleFontSize});

	// Side by side on the island's beach, below the crab.
	const float buttonsTop = h * 0.78F;
	std::ignore = ui::CreateButton(
					  world,
					  {.x = w / 2.0F - kButtonWidth - kButtonGap / 2.0F, .y = buttonsTop, .w = kButtonWidth, .h = kButtonHeight},
					  "Play",
					  [onPlay = onPlayPressed](flecs::entity) {
						  if (onPlay) {
							  onPlay();
						  }
					  }
	)
					  .set_name("PlayButton")
					  .child_of(uiRoot_);

	std::ignore = ui::CreateButton(
					  world,
					  {.x = w / 2.0F + kButtonGap / 2.0F, .y = buttonsTop, .w = kButtonWidth, .h = kButtonHeight},
					  "Credits",
					  [this](flecs::entity) { creditsModal_.get_mut<ui::Modal>().open = true; }
	)
					  .set_name("CreditsButton")
					  .child_of(uiRoot_);
	BuildCreditsModal(world, w, h);

	// A whole set, not get_mut, for the smaller style: Load() runs deferred, so CreateButton's Button isn't readable yet.
	ui::Button smallStyle = world.get<ui::UIConfig>().button;
	smallStyle.label = "Settings";
	smallStyle.font_size = kSmallButtonFontSize;
	smallStyle.border_thickness = kSmallButtonBorder;
	std::ignore = ui::CreateButton(
					  world,
					  {.x = (w - kSmallButtonWidth) / 2.0F, .y = buttonsTop + kButtonHeight + kSmallButtonGap, .w = kSmallButtonWidth, .h = kSmallButtonHeight},
					  "Settings",
					  [this](flecs::entity) { settingsModal_.get_mut<ui::Modal>().open = true; }
	)
					  .set<ui::Button>(smallStyle)
					  .set_name("SettingsButton")
					  .child_of(uiRoot_);
	settingsModal_ = BuildSettingsModal(world, uiRoot_, w, h);
}

void TitleScene::BuildCreditsModal(const flecs::world& world, const float width, const float height) {
	const float x = (width - kCreditsWidth) / 2.0F;
	const float y = (height - kCreditsHeight) / 2.0F;
	creditsModal_ = ui::CreateModal(world, {.x = x, .y = y, .w = kCreditsWidth, .h = kCreditsHeight}, {.open = false})
						.set_name("CreditsModal")
						.child_of(uiRoot_);
	std::ignore = ui::CreateLabel(world, {.x = x, .y = y + 16.0F, .w = kCreditsWidth, .h = 44.0F}, "Credits", 40.0F)
					  .child_of(creditsModal_);

	constexpr std::array<std::string_view, 7> kLines{
		"Title music: \"Menu music\" by mrpoly (CC0), opengameart.org/content/menu-music",
		"Run music: \"Underwater\" by Cleyton Kauffman (CC0), opengameart.org/content/underwater-theme",
		"Game-over music: \"Emotional piano loop\" by extenz (CC0), opengameart.org/content/emotional-piano-loop",
		"Sound effects: generated for this game (CC0)",
		"Font: Lilita One by Juan Montoreano (SIL Open Font License)",
		"Built with raylib, Flecs, Dear ImGui, nlohmann/json, spdlog, GLM, GLFW, stb, Jolt Physics and AngelScript",
		"Made with the help of Claude Opus and Claude Sonnet by Anthropic",
	};
	float top = y + 76.0F;
	for (const std::string_view line : kLines) {
		const std::string text{line};
		std::ignore = ui::CreateLabel(world, {.x = x + 24.0F, .y = top, .w = kCreditsWidth - 48.0F, .h = kCreditsLineHeight}, text, 20.0F)
						  .set<ui::Label>({.text = text, .font_size = 20.0F, .color = kPanelTextColor, .wrap = true})
						  .child_of(creditsModal_);
		top += kCreditsLineHeight + 8.0F;
	}

	std::ignore = ui::CreateButton(
					  world,
					  {.x = x + (kCreditsWidth - 200.0F) / 2.0F, .y = y + kCreditsHeight - 76.0F, .w = 200.0F, .h = 56.0F},
					  "Back",
					  [this](flecs::entity) { creditsModal_.get_mut<ui::Modal>().open = false; }
	)
					  .set_name("CreditsBackButton")
					  .child_of(creditsModal_);
}

void TitleScene::Unload(flecs::world&) {
	if (backdrop_) {
		backdrop_.destruct();
		backdrop_ = {};
	}
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = {};
		creditsModal_ = {};
		settingsModal_ = {};
	}
}

} // namespace crabnado
