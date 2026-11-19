#include <memory>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

using engine::input::InputState;
using engine::input::UICapture;

namespace {

// A headless platform whose mouse the test drives.
class MousePlatform : public engine::platform::HeadlessPlatform {
public:
	glm::vec2 position{};
	glm::vec2 wheel{};
	bool left_down{false};
	bool d_down{false};

	[[nodiscard]] glm::vec2 MousePosition() const override { return position; }
	[[nodiscard]] glm::vec2 MouseWheel() const override { return wheel; }
	[[nodiscard]] bool IsKeyDown(const int key) const override { return key == engine::input::KeyCode::D && d_down; }
	[[nodiscard]] bool IsMouseButtonDown(const int button) const override { return button == 0 && left_down; }
};

struct CaptureWorld {
	std::unique_ptr<engine::EngineContext> ctx;
	MousePlatform* platform;
	flecs::world& world;
	flecs::entity button;
	int clicks{0};

	CaptureWorld() :
		ctx(std::make_unique<engine::EngineContext>(std::make_unique<MousePlatform>())),
		platform(static_cast<MousePlatform*>(ctx->GetPlatform())),
		world(*ctx->GetWorld()) {
		button = engine::ui::CreateButton(world, {.x = 100.0F, .y = 100.0F, .w = 80.0F, .h = 30.0F}, "Go", [this](auto) {
			++clicks;
		});
	}

	// What gameplay (an OnUpdate system after the UI) sees this Frame.
	InputState Frame() {
		InputState seen{};
		const auto probe = world.system("Probe").kind(flecs::OnUpdate).run([&](const flecs::iter& it) {
			seen = it.world().get<InputState>();
		});
		engine::fixed_step::RunFrame(world, 1.0F / 60.0F);
		probe.destruct();
		return seen;
	}

	void OverButton() { platform->position = {120.0F, 110.0F}; }
	void Away() { platform->position = {500.0F, 400.0F}; }
};

TEST(UIPointerCapture, HoveringAButtonSetsPointerCapture) {
	CaptureWorld w;
	w.OverButton();
	w.Frame();

	EXPECT_TRUE(w.world.get<UICapture>().pointer);
	w.Away();
	w.Frame();
	EXPECT_FALSE(w.world.get<UICapture>().pointer);
}

TEST(UIPointerCapture, ClickingAButtonClicksItWithoutGameplaySeeingThePress) {
	CaptureWorld w;
	w.OverButton();
	w.Frame(); // the hover registers; capture takes effect next Frame

	w.platform->left_down = true;
	const auto press = w.Frame();
	EXPECT_FALSE(press.mouse.left.pressed);
	EXPECT_FALSE(press.mouse.left.state);
	EXPECT_EQ(press.mouse.window_position.x, 120.0F) << "position still flows";

	w.platform->left_down = false;
	w.Frame();
	EXPECT_EQ(w.clicks, 1);
}

TEST(UIPointerCapture, HoverZeroesScrollForGameplay) {
	CaptureWorld w;
	w.OverButton();
	w.Frame();

	w.platform->wheel = {0.0F, 3.0F};
	EXPECT_EQ(w.Frame().mouse.scroll.y, 0.0F);
}

TEST(UIPointerCapture, ClickAwayFromUiReachesGameplay) {
	CaptureWorld w;
	w.Away();
	w.Frame();

	w.platform->left_down = true;
	const auto press = w.Frame();
	EXPECT_TRUE(press.mouse.left.pressed);
	EXPECT_EQ(w.clicks, 0);
}

TEST(UIPointerCapture, DragStartedOnUiStaysCapturedUntilRelease) {
	CaptureWorld w;
	w.OverButton();
	w.Frame();
	w.platform->left_down = true;
	w.Frame();

	w.Away(); // dragged off the button, still held
	const auto drag = w.Frame();
	EXPECT_FALSE(drag.mouse.left.state);

	w.platform->left_down = false;
	w.Frame();
	w.platform->left_down = true;
	const auto next_press = w.Frame();
	EXPECT_TRUE(next_press.mouse.left.pressed) << "a fresh press off the UI is gameplay's";
}

TEST(UIPointerCapture, GameDragOntoAButtonIsNotStolen) {
	CaptureWorld w;
	w.Away();
	w.Frame();
	w.platform->left_down = true;
	w.Frame();

	w.OverButton();
	w.Frame();
	const auto over = w.Frame();
	EXPECT_TRUE(over.mouse.left.state) << "the drag began on gameplay and stays there";
	EXPECT_EQ(w.clicks, 0) << "and the button doesn't see a press it didn't start";
}

TEST(UIPointerCapture, OpenModalCapturesThePointerAnywhere) {
	CaptureWorld w;
	const auto modal = engine::ui::CreateModal(w.world, {.x = 300.0F, .y = 200.0F, .w = 200.0F, .h = 100.0F});
	w.Away();
	w.Frame();
	EXPECT_TRUE(w.world.get<UICapture>().pointer);

	modal.get_mut<engine::ui::Modal>().open = false;
	w.Frame();
	EXPECT_FALSE(w.world.get<UICapture>().pointer);
}

TEST(UIPointerCapture, LockedCursorSkipsTheGate) {
	CaptureWorld w;
	w.OverButton();
	w.Frame();
	w.platform->SetCursorLocked(true);
	w.platform->left_down = true;

	EXPECT_TRUE(w.Frame().mouse.left.pressed);
}

TEST(UIPointerCapture, KeyboardCaptureWithholdsKeysFromGameplay) {
	CaptureWorld w;
	w.platform->d_down = true;
	w.world.set<UICapture>({.pointer = false, .keyboard = true});

	EXPECT_FALSE(w.Frame().IsKeyDown(engine::input::KeyCode::D));

	w.world.set<UICapture>({});
	EXPECT_TRUE(w.Frame().IsKeyDown(engine::input::KeyCode::D));
}

} // namespace
