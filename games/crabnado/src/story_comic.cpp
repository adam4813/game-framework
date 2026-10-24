#include "story_comic.hpp"

#include <algorithm>
#include <string_view>

#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

namespace {

// The art's sizes: the page fills a 1280x720 screen and the panels sit in a row under its masthead. Textures draw 1:1.
constexpr float kPageWidth = 1280.0F;
constexpr float kPageHeight = 720.0F;
constexpr float kPanelWidth = 392.0F;
constexpr float kPanelHeight = 540.0F;
constexpr float kPanelsLeft = 34.0F;
constexpr float kPanelsTop = 90.0F;
constexpr float kPanelGap = 18.0F;
constexpr std::string_view kPagePath = "textures/crabnado/story/page.png";
constexpr std::array<std::string_view, StoryComic::kPanelCount> kPanelPaths{
	"textures/crabnado/story/panel_1.png", "textures/crabnado/story/panel_2.png", "textures/crabnado/story/panel_3.png"
};

// Above the title's UI and below its modals.
constexpr int kZIndex = 50;
// The page fades in over kPageFadeTime seconds; a panel fades in over kPanelFadeTime as it drops kPanelDrop pixels.
constexpr float kPageFadeTime = 0.9F;
constexpr float kPanelFadeTime = 0.35F;
constexpr float kPanelDrop = 18.0F;

constexpr float kButtonWidth = 180.0F;
constexpr float kButtonHeight = 50.0F;
constexpr float kSkipWidth = 120.0F;
constexpr float kSkipHeight = 40.0F;
constexpr float kButtonsBottom = 12.0F;

void SetAlpha(const flecs::entity panel, const float alpha) {
	panel.get_mut<ui::Panel>().color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0F, 1.0F) * 255.0F);
}

void SetVisible(const flecs::entity entity, const bool visible) { entity.get_mut<ui::UIElement>().visible = visible; }

} // namespace

void StoryComic::Build(const flecs::world& world, const flecs::entity parent, const float width, const float height) {
	started_ = false;
	finished_ = false;
	pageFade_ = 0.0F;
	shown_ = 0;
	panelFade_ = {};

	const float left = (width - kPageWidth) / 2.0F;
	const float top = (height - kPageHeight) / 2.0F;
	const auto texture = [&](const std::string_view path) {
		std::string resolved = assets::ResolveAsset(world, path);
		const int handle = assets::LoadTexture(world, resolved);
		textures_.push_back(std::move(resolved));
		return handle;
	};
	// A textured panel tinted fully transparent, so it starts hidden and fades in through its alpha.
	const auto picture = [&](const char* name, const flecs::entity owner, const core::Rect rect, const std::string_view path) {
		return world.entity(name)
			.child_of(owner)
			.set<ui::UIRect>({rect})
			.set<ui::Panel>(
				{.color = {.r = 255, .g = 255, .b = 255, .a = 0}, .border_thickness = 0.0F, .texture = texture(path)}
			);
	};

	root_ = world.entity("StoryComic")
				.child_of(parent)
				.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = width, .h = height}})
				.set<ui::UIElement>({.z_index = kZIndex, .visible = false});
	page_ = picture("Page", root_, {.x = left, .y = top, .w = kPageWidth, .h = kPageHeight}, kPagePath);
	for (int i = 0; i < kPanelCount; ++i) {
		const float x = left + kPanelsLeft + static_cast<float>(i) * (kPanelWidth + kPanelGap);
		const std::string name = "Panel" + std::to_string(i + 1);
		panels_[i] = picture(name.c_str(), page_, {.x = x, .y = top + kPanelsTop, .w = kPanelWidth, .h = kPanelHeight}, kPanelPaths[i]);
	}

	const float buttonsY = top + kPageHeight - kButtonsBottom - kButtonHeight;
	nextButton_ = ui::CreateButton(
					  world,
					  {.x = left + kPageWidth - kPanelsLeft - kButtonWidth, .y = buttonsY, .w = kButtonWidth, .h = kButtonHeight},
					  "Next",
					  [this](flecs::entity) { Next(); }
	)
					  .set_name("NextButton")
					  .set<ui::UIElement>({.visible = false})
					  .child_of(root_);

	ui::Button skipStyle = world.get<ui::UIConfig>().button;
	skipStyle.label = "Skip";
	skipStyle.font_size = 20.0F;
	skipStyle.border_thickness = 3.0F;
	skipButton_ = ui::CreateButton(
					  world,
					  {.x = left + kPanelsLeft, .y = buttonsY + (kButtonHeight - kSkipHeight) / 2.0F, .w = kSkipWidth, .h = kSkipHeight},
					  "Skip",
					  [this](flecs::entity) { Skip(); }
	)
					  .set<ui::Button>(skipStyle)
					  .set_name("SkipButton")
					  .set<ui::UIElement>({.visible = false})
					  .child_of(root_);
}

void StoryComic::Release(const flecs::world& world) {
	for (const std::string& path : textures_) {
		assets::Release(world, assets::AssetType::Texture, path);
	}
	textures_.clear();
	root_ = {};
	page_ = {};
	panels_ = {};
	nextButton_ = {};
	skipButton_ = {};
}

void StoryComic::Start() {
	if (started_) {
		return;
	}
	started_ = true;
	SetVisible(root_, true);
}

void StoryComic::Next() {
	if (!started_ || finished_) {
		return;
	}
	if (shown_ < kPanelCount) {
		++shown_;
		if (shown_ == kPanelCount) {
			nextButton_.get_mut<ui::Button>().label = "Fight!";
		}
		return;
	}
	finished_ = true;
}

void StoryComic::Skip() {
	if (started_) {
		finished_ = true;
	}
}

void StoryComic::Tick(const flecs::world& world, const float deltaTime) {
	if (!started_ || finished_) {
		return;
	}
	pageFade_ = std::min(pageFade_ + deltaTime / kPageFadeTime, 1.0F);
	SetAlpha(page_, pageFade_);
	if (pageFade_ < 1.0F) {
		return;
	}

	// The first panel and the buttons arrive as the page settles.
	if (shown_ == 0) {
		shown_ = 1;
		SetVisible(nextButton_, true);
		SetVisible(skipButton_, true);
	}
	const float panelsTop = page_.get<ui::UIRect>().rect.y + kPanelsTop;
	for (int i = 0; i < shown_; ++i) {
		panelFade_[i] = std::min(panelFade_[i] + deltaTime / kPanelFadeTime, 1.0F);
		const float ease = 1.0F - (1.0F - panelFade_[i]) * (1.0F - panelFade_[i]);
		SetAlpha(panels_[i], panelFade_[i]);
		panels_[i].get_mut<ui::UIRect>().rect.y = panelsTop - (1.0F - ease) * kPanelDrop;
	}

	const auto& input = world.get<input::InputState>();
	if (input.WasKeyPressed(input::KeyCode::Escape)) {
		Skip();
	}
	else if (input.WasKeyPressed(input::KeyCode::Space) || input.WasKeyPressed(input::KeyCode::Return)) {
		Next();
	}
}

} // namespace crabnado
