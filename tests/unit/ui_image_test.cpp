#include <gtest/gtest.h>

#include <memory>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "engine/engine.hpp"

using engine::core::Rect;
using engine::core::Rgba;
using engine::ui::FitMode;
using engine::ui::Image;
using engine::ui::UIDrawCommand;
using engine::ui::UIDrawKind;
using engine::ui::UIDrawList;

namespace {

constexpr int kWideTexture = 7; // 64 x 32
constexpr Rgba kWhite{.r = 255, .g = 255, .b = 255, .a = 255};

// One DrawTexturedRect the flush issued.
struct TexturedDraw {
	int texture{-1};
	Rect src{};
	Rect dest{};
	Rgba tint{};
};

// A headless platform with one known texture, recording the textured draws and scissors the UI flush issues.
class ImagePlatform : public engine::platform::HeadlessPlatform {
public:
	std::vector<TexturedDraw> draws;
	std::vector<Rect> scissors;

	int LoadTexture(std::string_view) override { return kWideTexture; }
	[[nodiscard]] glm::vec2 TextureSize(const int handle) const override {
		return handle == kWideTexture ? glm::vec2{64.0F, 32.0F} : glm::vec2{0.0F};
	}
	void DrawTexturedRect(const int texture, const Rect src, const Rect dest, const Rgba tint) override {
		draws.push_back({.texture = texture, .src = src, .dest = dest, .tint = tint});
	}
	void BeginScissor(const Rect r) override { scissors.push_back(r); }
};

void ExpectRect(const Rect actual, const Rect expected) {
	EXPECT_FLOAT_EQ(actual.x, expected.x);
	EXPECT_FLOAT_EQ(actual.y, expected.y);
	EXPECT_FLOAT_EQ(actual.w, expected.w);
	EXPECT_FLOAT_EQ(actual.h, expected.h);
}

struct UiWorld {
	std::unique_ptr<engine::EngineContext> ctx;
	ImagePlatform* platform{nullptr};

	UiWorld() {
		auto owned = std::make_unique<ImagePlatform>();
		platform = owned.get();
		ctx = std::make_unique<engine::EngineContext>(std::move(owned));
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	flecs::entity Add(const char* name, const Rect rect, const Image& image) const {
		return World().entity(name).set<engine::ui::UIRect>({rect}).set<Image>(image);
	}

	void RunSystem(const std::string_view name) const {
		flecs::entity found;
		World().query_builder<>().with(flecs::System).build().each([&](const flecs::entity e) {
			if (e.name() && std::string_view{e.name().c_str()} == name) {
				found = e;
			}
		});
		ASSERT_TRUE(found) << "no system named " << name;
		World().system(found).run();
	}

	// The textured commands the build pass queued.
	std::vector<UIDrawCommand> Build() const {
		RunSystem("UIBuildDrawList");
		std::vector<UIDrawCommand> out;
		for (const auto& c : World().get<UIDrawList>().commands) {
			if (c.kind == UIDrawKind::TexturedRect) {
				out.push_back(c);
			}
		}
		return out;
	}
};

TEST(UiImage, StretchFillsTheRectWithTheWholeTexture) {
	const UiWorld w;
	w.Add("Icon", {.x = 10, .y = 20, .w = 100, .h = 100}, {.texture = kWideTexture, .fit = FitMode::Stretch});

	const auto cmds = w.Build();

	ASSERT_EQ(cmds.size(), 1U);
	ExpectRect(cmds[0].rect, {.x = 10, .y = 20, .w = 100, .h = 100});
	ExpectRect(cmds[0].src, {.x = 0, .y = 0, .w = 64, .h = 32});
	EXPECT_EQ(cmds[0].texture, kWideTexture);
}

TEST(UiImage, FitLetterboxesKeepingTheAspectRatio) {
	const UiWorld w;
	w.Add("Icon", {.x = 10, .y = 20, .w = 100, .h = 100}, {.texture = kWideTexture, .fit = FitMode::Fit});

	const auto cmds = w.Build();

	ASSERT_EQ(cmds.size(), 1U);
	// 64x32 scales by 100/64 to 100x50, centered vertically in the 100x100 rect.
	ExpectRect(cmds[0].rect, {.x = 10, .y = 45, .w = 100, .h = 50});
	ExpectRect(cmds[0].src, {.x = 0, .y = 0, .w = 64, .h = 32});
}

TEST(UiImage, FillCropsTheTextureKeepingTheAspectRatio) {
	const UiWorld w;
	w.Add("Icon", {.x = 10, .y = 20, .w = 100, .h = 100}, {.texture = kWideTexture, .fit = FitMode::Fill});

	const auto cmds = w.Build();

	ASSERT_EQ(cmds.size(), 1U);
	ExpectRect(cmds[0].rect, {.x = 10, .y = 20, .w = 100, .h = 100});
	// The 100x100 rect shows a 32x32 square of the 64x32 texture, from its middle.
	ExpectRect(cmds[0].src, {.x = 16, .y = 0, .w = 32, .h = 32});
}

TEST(UiImage, TileRepeatsTheTextureAtOneTexelPerPixel) {
	const UiWorld w;
	w.Add("Icon", {.x = 0, .y = 0, .w = 150, .h = 90}, {.texture = kWideTexture, .fit = FitMode::Tile});

	const auto cmds = w.Build();

	ASSERT_EQ(cmds.size(), 1U);
	ExpectRect(cmds[0].rect, {.x = 0, .y = 0, .w = 150, .h = 90});
	ExpectRect(cmds[0].src, {.x = 0, .y = 0, .w = 150, .h = 90});
}

TEST(UiImage, ASourceRectPicksAnAtlasIcon) {
	const UiWorld w;
	w.Add(
		"Icon",
		{.x = 0, .y = 0, .w = 64, .h = 64},
		{.texture = kWideTexture, .fit = FitMode::Stretch, .source = {.x = 32, .y = 0, .w = 32, .h = 32}}
	);

	const auto cmds = w.Build();

	ASSERT_EQ(cmds.size(), 1U);
	ExpectRect(cmds[0].src, {.x = 32, .y = 0, .w = 32, .h = 32});
}

TEST(UiImage, FitUsesTheSourceRectsAspectRatioNotTheWholeTextures) {
	const UiWorld w;
	w.Add(
		"Icon",
		{.x = 0, .y = 0, .w = 80, .h = 40},
		{.texture = kWideTexture, .fit = FitMode::Fit, .source = {.x = 0, .y = 0, .w = 32, .h = 32}}
	);

	const auto cmds = w.Build();

	ASSERT_EQ(cmds.size(), 1U);
	ExpectRect(cmds[0].rect, {.x = 20, .y = 0, .w = 40, .h = 40});
}

TEST(UiImage, TintModulatesTheTexture) {
	const UiWorld w;
	const Rgba red{.r = 255, .g = 40, .b = 40, .a = 128};
	w.Add("Icon", {.x = 0, .y = 0, .w = 10, .h = 10}, {.texture = kWideTexture, .tint = red});

	const auto cmds = w.Build();

	ASSERT_EQ(cmds.size(), 1U);
	EXPECT_EQ(cmds[0].color.g, 40);
	EXPECT_EQ(cmds[0].color.a, 128);
}

TEST(UiImage, AnImageWithoutATextureDrawsNothing) {
	const UiWorld w;
	w.Add("Icon", {.x = 0, .y = 0, .w = 10, .h = 10}, {});

	EXPECT_TRUE(w.Build().empty());
}

TEST(UiImage, AnImageUnderAScrollRectIsOffsetByTheScrollAndClipped) {
	const UiWorld w;
	const auto scroll = w.World()
							.entity("Scroll")
							.set<engine::ui::UIRect>({{.x = 0, .y = 0, .w = 100, .h = 100}})
							.set<engine::ui::ScrollRect>({.scroll = {0.0F, 30.0F}});
	w.Add("Icon", {.x = 5, .y = 50, .w = 20, .h = 20}, {.texture = kWideTexture}).child_of(scroll);

	w.RunSystem("UIBuildDrawList");
	const auto& commands = w.World().get<UIDrawList>().commands;

	int begin = -1;
	int image = -1;
	int end = -1;
	for (int i = 0; i < static_cast<int>(commands.size()); ++i) {
		switch (commands[static_cast<size_t>(i)].kind) {
		case UIDrawKind::BeginClip: begin = i; break;
		case UIDrawKind::TexturedRect: image = i; break;
		case UIDrawKind::EndClip: end = i; break;
		default: break;
		}
	}
	ASSERT_GE(begin, 0);
	ASSERT_GT(image, begin);
	ASSERT_GT(end, image);
	ExpectRect(commands[static_cast<size_t>(image)].rect, {.x = 5, .y = 20, .w = 20, .h = 20});
}

TEST(UiImage, AnImageInsideAModalPaintsOnTheModalsLayer) {
	const UiWorld w;
	const auto modal = w.World()
						   .entity("Modal")
						   .set<engine::ui::UIRect>({{.x = 0, .y = 0, .w = 200, .h = 200}})
						   .set<engine::ui::UIElement>({.z_index = 50})
						   .set<engine::ui::Modal>({.open = true});
	w.Add("Icon", {.x = 5, .y = 5, .w = 20, .h = 20}, {.texture = kWideTexture}).child_of(modal);

	const auto cmds = w.Build();

	ASSERT_EQ(cmds.size(), 1U);
	EXPECT_EQ(cmds[0].layer, 50);
}

TEST(UiImage, ExistingPanelTexturesStillTileAtOneTexelPerPixel) {
	const UiWorld w;
	w.World()
		.entity("Backdrop")
		.set<engine::ui::UIRect>({{.x = 4, .y = 6, .w = 150, .h = 90}})
		.set<engine::ui::Panel>({.texture = kWideTexture});

	const auto cmds = w.Build();

	ASSERT_EQ(cmds.size(), 1U);
	ExpectRect(cmds[0].rect, {.x = 4, .y = 6, .w = 150, .h = 90});
	ExpectRect(cmds[0].src, {.x = 0, .y = 0, .w = 150, .h = 90});
}

TEST(UiImage, TheFlushHandsThePlatformTheResolvedSourceAndDestination) {
	const UiWorld w;
	w.Add("Icon", {.x = 10, .y = 20, .w = 100, .h = 100}, {.texture = kWideTexture, .fit = FitMode::Fit});

	w.RunSystem("UIBuildDrawList");
	w.RunSystem("UIRenderDrawList");

	ASSERT_EQ(w.platform->draws.size(), 1U);
	ExpectRect(w.platform->draws[0].dest, {.x = 10, .y = 45, .w = 100, .h = 50});
	ExpectRect(w.platform->draws[0].src, {.x = 0, .y = 0, .w = 64, .h = 32});
	EXPECT_EQ(w.platform->draws[0].tint.r, kWhite.r);
}

TEST(UiImage, AnImageAuthoredInLevelJsonLoadsItsFitSourceAndTint) {
	const UiWorld w;
	const char* level = R"({"entities": [{"name": "Crosshair", "components": {
		"ui_rect": [0, 0, 32, 32],
		"image": {"texture": "ui/atlas.png", "fit": "fill", "source": [32, 0, 32, 32], "tint": [255, 0, 0, 200]}
	}}]})";

	engine::level::LoadLevelFromSource(w.World(), level);

	const auto e = w.World().lookup("Crosshair");
	ASSERT_TRUE(e.is_valid());
	const auto& image = e.get<Image>();
	EXPECT_EQ(image.texture, kWideTexture);
	EXPECT_EQ(image.fit, FitMode::Fill);
	ExpectRect(image.source, {.x = 32, .y = 0, .w = 32, .h = 32});
	EXPECT_EQ(image.tint.g, 0);
	EXPECT_EQ(image.tint.a, 200);
}

TEST(UiImage, AnUnknownFitNameInJsonKeepsTheDefault) {
	const UiWorld w;
	engine::level::LoadLevelFromSource(
		w.World(),
		R"({"entities": [{"name": "Icon", "components": {"ui_rect": [0, 0, 8, 8], "image": {"fit": "sideways"}}}]})"
	);

	EXPECT_EQ(w.World().lookup("Icon").get<Image>().fit, Image{}.fit);
}

} // namespace
