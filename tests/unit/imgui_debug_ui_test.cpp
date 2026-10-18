#include <gtest/gtest.h>
#include <imgui.h>

#include "engine/platform/imgui/imgui_debug_ui.hpp"

using engine::platform::ImGuiDebugUi;

namespace {

constexpr glm::vec2 kDisplay{1280.0f, 720.0f};
constexpr float kDt = 1.0f / 60.0f;

class ImGuiDebugUiTest : public ::testing::Test {
protected:
	void SetUp() override { ImGui::GetIO().IniFilename = nullptr; }

	// Widget centres in the test panel, as laid out by the last frame.
	struct Layout {
		glm::vec2 field{};
		glm::vec2 button{};
	};

	static glm::vec2 LastItemCentre() {
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
	}

	// Runs one frame with a single panel holding a text field and a button, queuing `mouse` (and an
	// optional left-button state) as this frame's input.
	Layout Frame(const glm::vec2 mouse, const bool mouse_down = false) {
		ImGuiIO& io = ImGui::GetIO();
		io.AddMousePosEvent(mouse.x, mouse.y);
		io.AddMouseButtonEvent(0, mouse_down);
		ui.BeginFrame(kDisplay, kDt);
		Layout layout;
		if (ui.BeginPanel("Demo")) {
			ui.Text("some content so the panel has a size");
			ui.InputText("name", text);
			layout.field = LastItemCentre();
			ui.Button("press");
			layout.button = LastItemCentre();
		}
		ui.EndPanel();
		ui.EndFrame();
		return layout;
	}

	// Settles the panel's auto-size (ImGui hides a new window for its first frames).
	Layout Settle() {
		Layout layout;
		for (int i = 0; i < 3; ++i) {
			layout = Frame(kOffPanel);
		}
		return layout;
	}

	static constexpr glm::vec2 kOffPanel{1200.0f, 700.0f};
	static constexpr glm::vec2 kOnPanelTitle{70.0f, 66.0f}; // new windows open at (60, 60)

	ImGuiDebugUi ui;
	std::string text;
};

TEST_F(ImGuiDebugUiTest, hidden_by_default_and_panels_do_not_open) {
	EXPECT_FALSE(ui.IsVisible());

	ui.BeginFrame(kDisplay, kDt);
	EXPECT_FALSE(ui.BeginPanel("Demo"));
	ui.Text("ignored while hidden");
	ui.EndPanel();

	EXPECT_FALSE(ui.EndFrame());
}

TEST_F(ImGuiDebugUiTest, visible_panel_opens_and_frame_produces_draw_data) {
	ui.SetVisible(true);

	ui.BeginFrame(kDisplay, kDt);
	EXPECT_TRUE(ui.BeginPanel("Demo"));
	ui.EndPanel();

	EXPECT_TRUE(ui.EndFrame());
	EXPECT_FALSE(ui.EndFrame()) << "a second EndFrame in the same frame is a no-op";
}

TEST_F(ImGuiDebugUiTest, captures_mouse_only_while_pointer_is_over_a_panel) {
	ui.SetVisible(true);
	Settle();

	Frame(kOffPanel);
	EXPECT_FALSE(ui.CapturesMouse());

	Frame(kOnPanelTitle);
	EXPECT_TRUE(ui.CapturesMouse());
}

TEST_F(ImGuiDebugUiTest, hidden_ui_never_captures_input) {
	ui.SetVisible(true);
	Settle();
	Frame(kOnPanelTitle);
	ASSERT_TRUE(ui.CapturesMouse());

	ui.SetVisible(false);
	Frame(kOnPanelTitle);
	EXPECT_FALSE(ui.CapturesMouse());
	EXPECT_FALSE(ui.CapturesKeyboard());
}

TEST_F(ImGuiDebugUiTest, focused_panel_alone_does_not_capture_keyboard) {
	ui.SetVisible(true);
	Settle();

	// Click the title bar: focuses the panel without activating a widget.
	Frame(kOnPanelTitle, true);
	Frame(kOnPanelTitle, false);
	Frame(kOffPanel);

	EXPECT_FALSE(ui.CapturesKeyboard()) << "keyboard stays with the game unless a text field is active";
}

TEST_F(ImGuiDebugUiTest, holding_a_button_does_not_capture_keyboard) {
	ui.SetVisible(true);
	const glm::vec2 button = Settle().button;

	Frame(button, true);
	Frame(button, true); // button is now the active widget

	EXPECT_TRUE(ui.CapturesMouse());
	EXPECT_FALSE(ui.CapturesKeyboard()) << "game keys keep working while a panel widget is held";
}

TEST_F(ImGuiDebugUiTest, active_text_field_captures_keyboard) {
	ui.SetVisible(true);
	const glm::vec2 field = Settle().field;

	Frame(field, true);
	Frame(field, false);
	Frame(kOffPanel);

	EXPECT_TRUE(ui.CapturesKeyboard());
}

} // namespace
