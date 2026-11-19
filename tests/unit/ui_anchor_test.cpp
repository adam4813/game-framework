#include <gtest/gtest.h>

#include <memory>
#include <string_view>

#include "engine/engine.hpp"

using engine::core::Rect;
using engine::ui::Stack;
using engine::ui::UIAnchor;
using engine::ui::UIRect;

namespace {

// A headless world (1280 x 720 canvas) with UILayout run by hand.
struct UiWorld {
	std::unique_ptr<engine::EngineContext> ctx{
		std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>())
	};

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	flecs::entity Element(const Rect rect, const UIAnchor anchor, const flecs::entity parent = {}) const {
		auto e = World().entity();
		if (parent) {
			e.child_of(parent);
		}
		e.set<UIRect>({rect});
		e.set<UIAnchor>(anchor);
		return e;
	}

	void Layout() const {
		flecs::entity found;
		World().query_builder<>().with(flecs::System).build().each([&](const flecs::entity e) {
			if (e.name() && std::string_view{e.name().c_str()} == "UILayout") {
				found = e;
			}
		});
		ASSERT_TRUE(found);
		World().system(found).run();
	}

	static Rect RectOf(const flecs::entity e) { return e.get<UIRect>().rect; }
};

constexpr float kCanvasW = 1280.0F;
constexpr float kCanvasH = 720.0F;

TEST(UIAnchor, RootsAnchorToTheCanvasWithPivotDefaultingToTheAnchor) {
	// Hotbar: centered at the bottom, 20 px up from the edge.
	const UiWorld w;
	const auto hotbar = w.Element({0, 0, 400, 60}, {.anchor = {0.5F, 1.0F}, .offset = {0, 20}});
	w.Layout();
	const Rect r = UiWorld::RectOf(hotbar);
	EXPECT_FLOAT_EQ(r.x, 440.0F); // 1280 * 0.5 - 0.5 * 400
	EXPECT_FLOAT_EQ(r.y, 640.0F); // 720 - 20 - 60
	EXPECT_FLOAT_EQ(r.w, 400.0F);
	EXPECT_FLOAT_EQ(r.h, 60.0F);
}

TEST(UIAnchor, TheOtherCornerAndEdgePlacements) {
	const UiWorld w;
	const auto top_left = w.Element({0, 0, 200, 40}, {.offset = {10, 10}});
	const auto top_center = w.Element({0, 0, 300, 30}, {.anchor = {0.5F, 0.0F}, .offset = {0, 8}});
	const auto left_column = w.Element({0, 0, 80, 200}, {.anchor = {0.0F, 0.5F}, .offset = {6, 0}});
	const auto bottom_right = w.Element({0, 0, 150, 24}, {.anchor = {1.0F, 1.0F}, .offset = {12, 12}});
	w.Layout();
	EXPECT_FLOAT_EQ(UiWorld::RectOf(top_left).x, 10.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(top_left).y, 10.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(top_center).x, 490.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(top_center).y, 8.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(left_column).x, 6.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(left_column).y, 260.0F); // 360 - 100
	EXPECT_FLOAT_EQ(UiWorld::RectOf(bottom_right).x, kCanvasW - 12.0F - 150.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(bottom_right).y, kCanvasH - 12.0F - 24.0F);
}

TEST(UIAnchor, OffsetMovesInwardFromTheAnchoredEdge) {
	const UiWorld w;
	const auto left = w.Element({0, 0, 10, 10}, {.anchor = {0, 0}, .pivot = {0, 0}, .offset = {5, 5}});
	const auto right = w.Element({0, 0, 10, 10}, {.anchor = {1, 1}, .pivot = {1, 1}, .offset = {5, 5}});
	w.Layout();
	EXPECT_FLOAT_EQ(UiWorld::RectOf(left).x, 5.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(left).y, 5.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(right).x, kCanvasW - 5.0F - 10.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(right).y, kCanvasH - 5.0F - 10.0F);
}

TEST(UIAnchor, ExplicitPivotOverridesThePerAxisDefault) {
	// Pivot x is explicit (0 = left edge); pivot y stays -1 (same as the anchor, 0.5 = centered).
	const UiWorld w;
	const auto e = w.Element({0, 0, 100, 50}, {.anchor = {0.5F, 0.5F}, .pivot = {0.0F, -1.0F}});
	w.Layout();
	EXPECT_FLOAT_EQ(UiWorld::RectOf(e).x, 640.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(e).y, 335.0F);
}

TEST(UIAnchor, ChildOfANonStackParentAnchorsToTheParentRect) {
	const UiWorld w;
	const auto parent = w.World().entity().set<UIRect>({{.x = 100, .y = 200, .w = 300, .h = 100}});
	const auto child = w.Element({0, 0, 50, 20}, {.anchor = {1, 1}, .offset = {10, 10}}, parent);
	w.Layout();
	EXPECT_FLOAT_EQ(UiWorld::RectOf(child).x, 100.0F + 300.0F - 10.0F - 50.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(child).y, 200.0F + 100.0F - 10.0F - 20.0F);
}

TEST(UIAnchor, ParentWithoutARectFallsBackToTheCanvas) {
	const UiWorld w;
	const auto parent = w.World().entity();
	const auto child = w.Element({0, 0, 100, 20}, {.anchor = {1, 0}}, parent);
	w.Layout();
	EXPECT_FLOAT_EQ(UiWorld::RectOf(child).x, kCanvasW - 100.0F);
}

TEST(UIAnchor, AnAnchoredStackIsPlacedBeforeItsChildrenLayOut) {
	const UiWorld w;
	const auto stack = w.Element({0, 0, 200, 100}, {.anchor = {1, 0}, .offset = {10, 10}});
	stack.set<Stack>({.spacing = 4.0F, .padding = 2.0F, .stretch = true});
	const auto a = w.World().entity().child_of(stack).set<UIRect>({{.x = 0, .y = 0, .w = 50, .h = 20}});
	const auto b = w.World().entity().child_of(stack).set<UIRect>({{.x = 0, .y = 0, .w = 50, .h = 20}});
	w.Layout();
	const float stack_x = kCanvasW - 10.0F - 200.0F;
	EXPECT_FLOAT_EQ(UiWorld::RectOf(stack).x, stack_x);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(a).x, stack_x + 2.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(a).y, 12.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(b).y, 12.0F + 20.0F + 4.0F);
}

TEST(UIAnchor, AStackChildIgnoresItsAnchor) {
	const UiWorld w;
	const auto stack = w.World().entity().set<UIRect>({{.x = 30, .y = 40, .w = 200, .h = 100}});
	stack.set<Stack>({.spacing = 0.0F, .padding = 0.0F, .stretch = false});
	const auto child = w.Element({0, 0, 50, 20}, {.anchor = {1, 1}, .offset = {5, 5}}, stack);
	w.Layout();
	EXPECT_FLOAT_EQ(UiWorld::RectOf(child).x, 30.0F);
	EXPECT_FLOAT_EQ(UiWorld::RectOf(child).y, 40.0F);
}

TEST(UIAnchor, LayoutIsIdempotentAcrossFrames) {
	const UiWorld w;
	const auto e = w.Element({0, 0, 100, 40}, {.anchor = {1, 1}, .offset = {10, 10}});
	w.Layout();
	const Rect first = UiWorld::RectOf(e);
	w.Layout();
	const Rect second = UiWorld::RectOf(e);
	EXPECT_FLOAT_EQ(first.x, second.x);
	EXPECT_FLOAT_EQ(first.y, second.y);
}

} // namespace
