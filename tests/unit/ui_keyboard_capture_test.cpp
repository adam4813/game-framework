#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

using engine::input::InputState;
namespace KeyCode = engine::input::KeyCode;
using engine::input::UICapture;

namespace {

// A headless platform whose mouse, keys and typed text the test drives.
class KeyboardPlatform : public engine::platform::HeadlessPlatform {
public:
	glm::vec2 position{};
	bool left_down{false};
	bool w_down{false};
	bool backspace_pressed{false}; // held and reported as a repeat this Frame
	bool return_down{false};
	std::string typed;

	[[nodiscard]] glm::vec2 MousePosition() const override { return position; }
	[[nodiscard]] bool IsMouseButtonDown(const int button) const override { return button == 0 && left_down; }
	[[nodiscard]] bool IsKeyDown(const int key) const override {
		return (key == KeyCode::W && w_down) || (key == KeyCode::Backspace && backspace_pressed)
			   || (key == KeyCode::Return && return_down);
	}
	[[nodiscard]] bool IsKeyPressedRepeat(const int key) const override {
		return key == KeyCode::Backspace && backspace_pressed;
	}
	[[nodiscard]] std::string TextInput() const override { return typed; }
};

struct KeyboardWorld {
	std::unique_ptr<engine::EngineContext> ctx;
	KeyboardPlatform* platform;
	flecs::world& world;
	flecs::entity field;
	std::string text;
	int submits{0};

	KeyboardWorld() :
		ctx(std::make_unique<engine::EngineContext>(std::make_unique<KeyboardPlatform>())),
		platform(static_cast<KeyboardPlatform*>(ctx->GetPlatform())),
		world(*ctx->GetWorld()) {
		field = engine::ui::CreateTextInput(
			world,
			{.x = 100.0F, .y = 100.0F, .w = 200.0F, .h = 30.0F},
			"",
			[this](const std::string& t) { text = t; }
		);
		field.get_mut<engine::ui::TextInput>().on_submit = [this](const std::string&) { ++submits; };
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

	// A press Frame then a release Frame at (x, y).
	void Click(const float x, const float y) {
		platform->position = {x, y};
		platform->left_down = true;
		Frame();
		platform->left_down = false;
		Frame();
	}
	void FocusField() { Click(150.0F, 110.0F); }
	void ClickAway() { Click(600.0F, 500.0F); }
};

TEST(UIKeyboardCapture, FocusingATextFieldSetsKeyboardCapture) {
	KeyboardWorld w;
	EXPECT_FALSE(w.world.get<UICapture>().keyboard);
	w.FocusField();
	EXPECT_TRUE(w.world.get<UICapture>().keyboard);
}

TEST(UIKeyboardCapture, TypingWasdIntoAFocusedFieldLeavesGameplayKeysUp) {
	KeyboardWorld w;
	w.FocusField();

	w.platform->w_down = true;
	w.platform->typed = "w";
	const auto seen = w.Frame();
	EXPECT_FALSE(seen.IsKeyDown(KeyCode::W));
	EXPECT_FALSE(seen.WasKeyPressed(KeyCode::W));
	EXPECT_EQ(w.text, "w") << "the field still gets the typed text";
}

TEST(UIKeyboardCapture, FieldStillGetsBackspaceAndReturn) {
	KeyboardWorld w;
	w.FocusField();
	w.platform->typed = "ab";
	w.Frame();
	w.platform->typed.clear();
	ASSERT_EQ(w.text, "ab");

	w.platform->backspace_pressed = true;
	const auto seen = w.Frame();
	EXPECT_FALSE(seen.IsKeyDown(KeyCode::Backspace));
	EXPECT_FALSE(seen.WasKeyPressedOrRepeated(KeyCode::Backspace));
	EXPECT_EQ(w.text, "a");
	w.platform->backspace_pressed = false;
	w.Frame();

	w.platform->return_down = true;
	const auto enter = w.Frame();
	EXPECT_FALSE(enter.IsKeyDown(KeyCode::Return));
	EXPECT_EQ(w.submits, 1);
}

TEST(UIKeyboardCapture, ClickingAwayRestoresGameplayNextFrame) {
	KeyboardWorld w;
	w.FocusField();
	w.ClickAway();
	EXPECT_FALSE(w.world.get<UICapture>().keyboard);

	w.platform->w_down = true;
	EXPECT_TRUE(w.Frame().IsKeyDown(KeyCode::W));
}

TEST(UIKeyboardCapture, UnfocusedFieldLeavesGameplayKeysAlone) {
	KeyboardWorld w;
	w.platform->w_down = true;
	EXPECT_TRUE(w.Frame().IsKeyDown(KeyCode::W));
}

TEST(UIKeyboardCapture, DestroyingTheFocusedFieldFreesTheKeyboard) {
	KeyboardWorld w;
	w.FocusField();
	w.field.destruct();
	w.Frame();
	EXPECT_FALSE(w.world.get<UICapture>().keyboard);
}

} // namespace
