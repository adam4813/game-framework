#include "title_scene.hpp"

#include <array>
#include <string>
#include <string_view>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr float kCreditsWidth = 900.0F;
constexpr float kCreditsHeight = 520.0F;
constexpr float kCreditsLineHeight = 52.0F;

} // namespace

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

	std::ignore = ui::CreateButton(
					  world,
					  {.x = w / 2.0F - 100.0F, .y = h / 2.0F + 72.0F, .w = 200.0F, .h = 56.0F},
					  "Credits",
					  [this](flecs::entity) { creditsModal_.get_mut<ui::Modal>().open = true; }
	)
					  .set_name("CreditsButton")
					  .child_of(uiRoot_);
	BuildCreditsModal(world, w, h);
}

void TitleScene::BuildCreditsModal(const flecs::world& world, const float width, const float height) {
	const float x = (width - kCreditsWidth) / 2.0F;
	const float y = (height - kCreditsHeight) / 2.0F;
	creditsModal_ = ui::CreateModal(world, {.x = x, .y = y, .w = kCreditsWidth, .h = kCreditsHeight}, {.open = false})
						.set_name("CreditsModal")
						.child_of(uiRoot_);
	std::ignore = ui::CreateLabel(world, {.x = x, .y = y + 16.0F, .w = kCreditsWidth, .h = 44.0F}, "Credits", 40.0F)
					  .child_of(creditsModal_);

	constexpr std::array<std::string_view, 6> kLines{
		"Title music: \"Menu music\" by mrpoly (CC0), opengameart.org/content/menu-music",
		"Run music: \"Underwater\" by Cleyton Kauffman (CC0), opengameart.org/content/underwater-theme",
		"Game-over music: \"Emotional piano loop\" by extenz (CC0), opengameart.org/content/emotional-piano-loop",
		"Sound effects: generated for this game (CC0)",
		"Built with raylib, Flecs, Dear ImGui, nlohmann/json, spdlog, GLM, GLFW, stb, Jolt Physics and AngelScript",
		"Made with the help of Claude Opus and Claude Sonnet by Anthropic",
	};
	float top = y + 76.0F;
	for (const std::string_view line : kLines) {
		const std::string text{line};
		std::ignore = ui::CreateLabel(world, {.x = x + 24.0F, .y = top, .w = kCreditsWidth - 48.0F, .h = kCreditsLineHeight}, text, 20.0F)
						  .set<ui::Label>({.text = text, .font_size = 20.0F, .wrap = true})
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
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = {};
		creditsModal_ = {};
	}
}

} // namespace crabnado
