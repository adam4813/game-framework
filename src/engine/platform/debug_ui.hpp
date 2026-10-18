#pragma once

#include <string>
#include <string_view>

#include "engine/core/core.hpp"

namespace engine::platform {

// Immediate-mode debug UI (tuning panels, debug buttons, editors). Game code draws through this
// interface so it never includes ImGui or Raylib; reach it via Platform::GetDebugUi().
//
// Frame model: the platform opens the debug UI frame in BeginFrame (before any phase), so panel
// systems may call widgets from any phase up to and including OnUpdate — run panels whose buttons
// change game state in OnUpdate so the change lands this frame. The engine renders the result in
// the ecs::OnDebugUI phase, on top of the scene and the 2D UI.
//
// Main thread only: ImGui's context is global and not thread-safe, so never mark a panel system
// .multi_threaded() (Flecs runs ordinary systems on the main thread even with set_threads > 1).
//
// This base class is also the null implementation used when the debug UI is compiled out
// (ENABLE_DEBUG_UI=OFF) or hidden: every widget is a no-op, BeginPanel returns false, and widgets
// that report interaction return false, so panel code needs no #ifdefs.
//
//   auto& ui = platform.GetDebugUi();
//   if (ui.BeginPanel("Faults")) {
//       if (ui.Button("Force jam")) { ... }
//   }
//   ui.EndPanel(); // always pair with BeginPanel, even when it returned false
class DebugUi {
public:
	virtual ~DebugUi() = default;

	// ── Visibility (toggled by input::kDebugUiToggleKey, which the game never sees) ──
	[[nodiscard]] virtual bool IsVisible() const { return false; }
	virtual void SetVisible(bool) {}

	// ── Input capture ──
	// True while the debug UI wants this frame's mouse (pointer over / dragging from a panel) or
	// keyboard (an ImGui text field is active). InputModule withholds that input from InputState so
	// the game doesn't see it.
	[[nodiscard]] virtual bool CapturesMouse() const { return false; }
	[[nodiscard]] virtual bool CapturesKeyboard() const { return false; }

	// ── Panels ──
	// Opens a movable window. Returns false when hidden or collapsed: skip its contents, but still
	// call EndPanel.
	virtual bool BeginPanel(std::string_view) { return false; }
	virtual void EndPanel() {}
	// Collapsible section inside a panel. Returns true when expanded.
	virtual bool CollapsingHeader(std::string_view) { return false; }

	// ── Widgets ──
	virtual void Text(std::string_view) {}
	virtual void TextColored(core::Rgba, std::string_view) {}
	virtual bool Button(std::string_view) { return false; }
	virtual bool Checkbox(std::string_view, bool&) { return false; }
	virtual bool SliderInt(std::string_view, int&, int, int) { return false; }
	virtual bool SliderFloat(std::string_view, float&, float, float) { return false; }
	virtual bool InputText(std::string_view, std::string&) { return false; }
	virtual bool Selectable(std::string_view, bool) { return false; }
	virtual void ProgressBar(float, std::string_view) {}
	virtual void Tooltip(std::string_view) {} // shown while the previous widget is hovered

	// ── Layout ──
	virtual void Separator() {}
	virtual void SameLine() {}
	virtual void PushId(int) {}
	virtual void PushId(std::string_view) {}
	virtual void PopId() {}
};

} // namespace engine::platform
