#ifdef ENGINE_DEBUG_UI

#include "imgui_debug_ui.hpp"

#include <string>

#include <imgui.h>
#include <imgui_stdlib.h>

namespace engine::platform {

namespace {

// Fallback frame time for a zero/negative dt (first frame); ImGui asserts on DeltaTime <= 0.
constexpr float kFallbackDeltaTime = 1.0f / 60.0f;

// ImGui takes NUL-terminated labels; debug UI call rates make the copy irrelevant.
std::string NulTerminated(const std::string_view s) { return std::string(s); }

} // namespace

ImGuiDebugUi::ImGuiDebugUi() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	// Build the font atlas up front so NewFrame never asserts; the backend uploads it as a texture.
	ImGui::GetIO().Fonts->Build();
}

ImGuiDebugUi::~ImGuiDebugUi() { ImGui::DestroyContext(); }

void ImGuiDebugUi::BeginFrame(const glm::vec2 display_size, const float dt) {
	capturesMouse_ = false;
	capturesKeyboard_ = false;
	if (!visible_) {
		return;
	}

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = {display_size.x, display_size.y};
	io.DeltaTime = dt > 0.0f ? dt : kFallbackDeltaTime;
	ImGui::NewFrame();
	frameActive_ = true;

	// NewFrame refreshes the Want* flags from this frame's queued input, so the game can be gated
	// on them before any system reads input. Keyboard uses WantTextInput, not WantCaptureKeyboard:
	// the latter is also true while any widget is active (dragging a slider, holding a button),
	// which would freeze game keys during ordinary panel use.
	capturesMouse_ = io.WantCaptureMouse;
	capturesKeyboard_ = io.WantTextInput;
}

bool ImGuiDebugUi::EndFrame() {
	if (!frameActive_) {
		return false;
	}
	frameActive_ = false;
	ImGui::Render();
	return true;
}

// ── Panels ──

bool ImGuiDebugUi::BeginPanel(const std::string_view title) {
	if (!frameActive_) {
		return false;
	}
	return ImGui::Begin(NulTerminated(title).c_str());
}

void ImGuiDebugUi::EndPanel() {
	if (frameActive_) {
		ImGui::End();
	}
}

bool ImGuiDebugUi::CollapsingHeader(const std::string_view label) {
	return frameActive_ && ImGui::CollapsingHeader(NulTerminated(label).c_str());
}

// ── Widgets ──

void ImGuiDebugUi::Text(const std::string_view text) {
	if (frameActive_) {
		ImGui::TextUnformatted(text.data(), text.data() + text.size());
	}
}

void ImGuiDebugUi::TextColored(const core::Rgba color, const std::string_view text) {
	if (!frameActive_) {
		return;
	}
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(color.r, color.g, color.b, color.a));
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::PopStyleColor();
}

bool ImGuiDebugUi::Button(const std::string_view label) { return frameActive_ && ImGui::Button(NulTerminated(label).c_str()); }

bool ImGuiDebugUi::Checkbox(const std::string_view label, bool& value) {
	return frameActive_ && ImGui::Checkbox(NulTerminated(label).c_str(), &value);
}

bool ImGuiDebugUi::SliderInt(const std::string_view label, int& value, const int min, const int max) {
	return frameActive_ && ImGui::SliderInt(NulTerminated(label).c_str(), &value, min, max);
}

bool ImGuiDebugUi::SliderFloat(const std::string_view label, float& value, const float min, const float max) {
	return frameActive_ && ImGui::SliderFloat(NulTerminated(label).c_str(), &value, min, max);
}

bool ImGuiDebugUi::InputText(const std::string_view label, std::string& value) {
	return frameActive_ && ImGui::InputText(NulTerminated(label).c_str(), &value);
}

bool ImGuiDebugUi::Selectable(const std::string_view label, const bool selected) {
	return frameActive_ && ImGui::Selectable(NulTerminated(label).c_str(), selected);
}

void ImGuiDebugUi::ProgressBar(const float fraction, const std::string_view label) {
	if (frameActive_) {
		ImGui::ProgressBar(fraction, {-1.0f, 0.0f}, label.empty() ? nullptr : NulTerminated(label).c_str());
	}
}

void ImGuiDebugUi::Tooltip(const std::string_view text) {
	if (frameActive_ && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", NulTerminated(text).c_str());
	}
}

// ── Layout ──

void ImGuiDebugUi::Separator() {
	if (frameActive_) {
		ImGui::Separator();
	}
}

void ImGuiDebugUi::SameLine() {
	if (frameActive_) {
		ImGui::SameLine();
	}
}

void ImGuiDebugUi::PushId(const int id) {
	if (frameActive_) {
		ImGui::PushID(id);
	}
}

void ImGuiDebugUi::PushId(const std::string_view id) {
	if (frameActive_) {
		ImGui::PushID(id.data(), id.data() + id.size());
	}
}

void ImGuiDebugUi::PopId() {
	if (frameActive_) {
		ImGui::PopID();
	}
}

} // namespace engine::platform

#endif // ENGINE_DEBUG_UI
