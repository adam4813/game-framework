#pragma once

#include <glm/glm.hpp>

#include "engine/platform/debug_ui.hpp"

namespace engine::platform {

// Dear ImGui implementation of DebugUi. Owns the ImGui context but knows nothing about the window
// backend: the backend queues input on ImGui's IO, calls BeginFrame before the world progresses,
// and EndFrame + renders the draw data during ecs::OnDebugUI.
class ImGuiDebugUi final : public DebugUi {
public:
	ImGuiDebugUi();
	~ImGuiDebugUi() override;
	ImGuiDebugUi(const ImGuiDebugUi&) = delete;
	ImGuiDebugUi& operator=(const ImGuiDebugUi&) = delete;

	// ── Frame (driven by the platform backend) ──
	// Starts an ImGui frame when visible; a no-op while hidden. Input for this frame must already
	// be queued on ImGui's IO.
	void BeginFrame(glm::vec2 display_size, float dt);
	// Finishes the frame. Returns true when ImGui draw data is ready to render; false when hidden or
	// the frame was already ended (safe to call more than once per frame).
	bool EndFrame();

	// ── DebugUi ──
	[[nodiscard]] bool IsVisible() const override { return visible_; }
	void SetVisible(bool visible) override { visible_ = visible; }
	// Valid after BeginFrame; always false while hidden.
	[[nodiscard]] bool CapturesMouse() const override { return capturesMouse_; }
	[[nodiscard]] bool CapturesKeyboard() const override { return capturesKeyboard_; }

	bool BeginPanel(std::string_view title) override;
	void EndPanel() override;
	bool CollapsingHeader(std::string_view label) override;

	void Text(std::string_view text) override;
	void TextColored(core::Rgba color, std::string_view text) override;
	bool Button(std::string_view label) override;
	bool Checkbox(std::string_view label, bool& value) override;
	bool SliderInt(std::string_view label, int& value, int min, int max) override;
	bool SliderFloat(std::string_view label, float& value, float min, float max) override;
	bool InputText(std::string_view label, std::string& value) override;
	bool Selectable(std::string_view label, bool selected) override;
	void ProgressBar(float fraction, std::string_view label) override;
	void Tooltip(std::string_view text) override;

	void Separator() override;
	void SameLine() override;
	void PushId(int id) override;
	void PushId(std::string_view id) override;
	void PopId() override;

private:
	bool visible_{false};
	bool frameActive_{false};
	bool capturesMouse_{false};
	bool capturesKeyboard_{false};
};

} // namespace engine::platform
