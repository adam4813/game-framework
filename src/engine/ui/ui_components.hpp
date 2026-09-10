#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/core/core.hpp"

// Data-driven 2D UI. An entity becomes a UI element by pairing a UIRect (its position + size)
// with one or more visual components (Panel, Label, Button). Behaviour is attached as extra
// components on the same entity (OnClick handler, audio::SoundEffect, ...), so a "button" is
// simply the composition UIRect + Button + OnClick + SoundEffect — no bespoke widget classes.
//
// Rendering is deferred: the visual systems never call the platform directly. They append draw
// commands to the UIDrawList singleton during the OnUI phase, then a single flush system issues
// the platform draw calls in order. Draw order is FIFO, refined by the entity hierarchy (a child
// composites on top of its parent) and an optional UIElement.z_index layer.
namespace engine::ui {

// Position + size of a UI element in screen-space pixels. Every UI entity carries exactly one.
// Wraps platform::Rect so hit-testing and the 2D draw calls share the same primitive.
struct UIRect {
	core::Rect rect{};

	[[nodiscard]] bool Contains(const glm::vec2 point) const { return rect.Contains(point); }
};

// Optional per-element metadata. Absent means visible with z_index 0. Higher z_index draws on
// top of lower ones regardless of hierarchy/FIFO position.
struct UIElement {
	int z_index{0};
	bool visible{true};
};

// Filled background box with an optional border. Used on its own for containers/backdrops or
// combined with a Label to make a titled panel.
struct Panel {
	core::Rgba color{core::colors::Panel};
	core::Rgba border_color{core::colors::Border};
	float border_thickness{2.0F};
	float roundness{0.0F}; // 0 = square corners; >0 = rounded (fraction of the shorter side)
	int texture{-1};       // optional texture handle (from platform LoadTexture); -1 = solid color fill
};

// Horizontal text placement of a Label within its UIRect.
enum class TextAlign : std::uint8_t { Left, Center, Right };

// Static text drawn (vertically centred) within the element's UIRect. When `wrap` is set the text is
// word-wrapped to the UIRect width and laid out top-aligned across multiple lines.
struct Label {
	std::string text;
	float font_size{20.0F};
	core::Rgba color{core::colors::Text};
	TextAlign align{TextAlign::Center};
	bool wrap{false};         // word-wrap to the UIRect width (multi-line, top-aligned)
	float line_spacing{4.0F}; // extra pixels between wrapped lines
};

// Clickable button. The stylable fields describe its look; the trailing fields are runtime state
// written each frame by UIButtonInteraction (never set them by hand). When a click completes the
// interaction system invokes the entity's OnClick handler and fires its audio::SoundEffect, if
// present.
struct Button {
	std::string label;
	float font_size{22.0F};
	core::Rgba normal{core::colors::Accent};
	core::Rgba hover{core::colors::PanelHi};
	core::Rgba pressed_color{core::colors::Panel};
	core::Rgba border_color{core::colors::Border};
	core::Rgba text_color{core::colors::Text};
	float roundness{0.25F};
	int texture{-1}; // optional background texture handle (from LoadTexture); -1 = solid color fill

	// === Runtime interaction state (written by UIButtonInteraction) ===
	bool hovered{false}; // cursor is over the button this frame
	bool pressed{false}; // press started on the button and the mouse is still held
	bool clicked{false}; // true only on the frame a click completes (press + release while hovered)
};

// Event handler invoked once when a Button on the same entity registers a completed click. The
// clicked entity is passed so one shared callback can service many buttons.
struct OnClick {
	std::function<void(flecs::entity)> callback;
};

// Global UI configuration singleton. Set once at game startup via ui::SetDefaultClickSound or by
// setting this singleton directly. The CreateButton factory reads it to auto-attach a SoundEffect.
struct UIConfig {
	std::string default_click_sound_path; // empty = no default sound
};

// Cached modal state singleton. Updated once per frame by UIUpdateModalState to avoid querying all
// modals for every button during interaction testing.
struct UIModalState {
	bool any_open{false};
};

// Programmatic click request. Add this tag to a Button entity (from a script, a test, or the
// flecs-api tool) to fire its OnClick handler + SoundEffect exactly as a real mouse click would,
// without synthesising mouse input. UIButtonClickRequest consumes and removes it each frame.
struct UIClickRequest {};

// Determinate value/progress bar. Non-interactive: game or script code drives `value`.
struct ProgressBar {
	float value{0.0F};
	float min{0.0F};
	float max{1.0F};
	core::Rgba fill_color{core::colors::Accent};
	core::Rgba track_color{core::colors::PanelBg};
	core::Rgba border_color{core::colors::Border};
	float border_thickness{2.0F};
	float roundness{0.35F};

	// Normalised fill in [0, 1].
	[[nodiscard]] float Fraction() const {
		const float range = max - min;
		if (range <= 0.0F) {
			return 0.0F;
		}
		return glm::clamp((value - min) / range, 0.0F, 1.0F);
	}
};

// Indeterminate loading spinner: `dots` dots rotating around the element's centre, trailing in
// opacity. `phase` is advanced every frame by UISpinnerAnimate.
struct Spinner {
	core::Rgba color{core::colors::Accent};
	float radius{18.0F};    // radius of the ring the dots travel on
	float dot_radius{4.0F}; // radius of each dot
	int dots{8};
	float speed{6.0F}; // angular speed in radians/second
	float phase{0.0F}; // current rotation, advanced by the animate system
};

// Layout direction for a Stack.
enum class StackDirection : std::uint8_t { Vertical, Horizontal };

// Auto-layout container: positions its direct UI children one after another along `direction`,
// starting at its own UIRect origin (+ padding), each separated by `spacing`. UIStackLayout writes
// the children's UIRect positions every frame; when `stretch` is set it also stretches them on the
// cross axis to fill the stack's width/height. The stack keeps each child's main-axis size.
struct Stack {
	StackDirection direction{StackDirection::Vertical};
	float spacing{8.0F};
	float padding{0.0F};
	bool stretch{true};
};

// Scrollable viewport. Clips its children to its own UIRect and offsets them by `-scroll`, so a
// (usually Stack-laid-out) child tree taller/wider than the viewport can be scrolled through. The
// mouse wheel scrolls while hovering; `content_size` is recomputed from the children each frame by
// UIScrollRectUpdate, which also clamps `scroll` and draws the scrollbar/thumb. The thumb can be
// clicked and dragged to scroll.
struct ScrollRect {
	glm::vec2 scroll{0.0F};
	glm::vec2 content_size{0.0F}; // auto-computed (unscrolled bounds of children)
	bool vertical{true};
	bool horizontal{false};
	float scrollbar_thickness{10.0F};
	float wheel_speed{28.0F};
	core::Rgba track_color{core::colors::PanelBg};
	core::Rgba thumb_color{core::colors::PanelHi};

	// === Runtime drag state (written by UIScrollRectUpdate) ===
	bool dragging_v{false};        // currently dragging the vertical thumb
	bool dragging_h{false};        // currently dragging the horizontal thumb
	float drag_start_mouse{0.0F};  // mouse axis position when drag began
	float drag_start_scroll{0.0F}; // scroll value when drag began
};

// Dialog overlay. When `open`, the element and its subtree render above everything else (use a
// high UIElement.z_index, set by CreateModal) behind a full-screen dimming backdrop, and — while
// any modal is open — only elements inside an open modal receive clicks. Closed modals skip both
// drawing and interaction for their whole subtree.
struct Modal {
	bool open{true};
	core::Rgba backdrop_color{6, 8, 6, 200};
	bool close_on_backdrop{false};
};

// === Draw list ===

// Kind of 2D primitive a queued UIDrawCommand represents.
enum class UIDrawKind : std::uint8_t {
	Rect,
	RoundedRect,
	RectLines,
	RoundedRectLines,
	Text,
	Line,
	Circle,
	TexturedRect, // a texture tiled into a rect, modulated by `color` (tint)
	BeginClip,    // push a scissor rectangle (intersected with any active clip)
	EndClip,      // pop the last scissor rectangle
};

// One queued 2D draw call. Only the fields relevant to `kind` are meaningful. `order` is the
// sort key (lower draws first); commands sharing an order keep their insertion order (FIFO).
struct UIDrawCommand {
	UIDrawKind kind{UIDrawKind::Rect};
	core::Rect rect{};      // rect-based kinds
	core::Rgba color{};     // all kinds
	float roundness{0.0F};  // (Rounded)Rect(Lines)
	float thickness{1.0F};  // *Lines, Line
	glm::vec2 p0{0.0F};     // Text position / Line start / Circle centre
	glm::vec2 p1{0.0F};     // Line end
	float radius{0.0F};     // Circle
	int texture{-1};        // TexturedRect (handle from LoadTexture)
	std::string text;       // Text
	float font_size{20.0F}; // Text
	int order{0};           // sort key (z_index)
};

// Singleton frame buffer of queued UI draw commands. Cleared and refilled every frame by the UI
// systems, then flushed to the platform. The Push* helpers only mutate their own storage, so
// game/scripting code may also enqueue custom overlay draws through the same list.
struct UIDrawList {
	std::vector<UIDrawCommand> commands;

	void Clear() { commands.clear(); }

	void PushRect(const core::Rect r, const core::Rgba color, const int order = 0) {
		commands.push_back({.kind = UIDrawKind::Rect, .rect = r, .color = color, .order = order});
	}

	void PushRoundedRect(const core::Rect r, const float roundness, const core::Rgba color, const int order = 0) {
		commands.push_back(
			{.kind = UIDrawKind::RoundedRect, .rect = r, .color = color, .roundness = roundness, .order = order}
		);
	}

	void PushRectLines(const core::Rect r, const float thickness, const core::Rgba color, const int order = 0) {
		commands.push_back(
			{.kind = UIDrawKind::RectLines, .rect = r, .color = color, .thickness = thickness, .order = order}
		);
	}

	void PushRoundedRectLines(
		const core::Rect r,
		const float roundness,
		const float thickness,
		const core::Rgba color,
		const int order = 0
	) {
		commands.push_back(
			{.kind = UIDrawKind::RoundedRectLines,
			 .rect = r,
			 .color = color,
			 .roundness = roundness,
			 .thickness = thickness,
			 .order = order}
		);
	}

	void PushText(
		std::string text,
		const float x,
		const float y,
		const float font_size,
		const core::Rgba color,
		const int order = 0
	) {
		commands.push_back(
			{.kind = UIDrawKind::Text,
			 .color = color,
			 .p0 = {x, y},
			 .text = std::move(text),
			 .font_size = font_size,
			 .order = order}
		);
	}

	void
	PushLine(const glm::vec2 a, const glm::vec2 b, const float thickness, const core::Rgba color, const int order = 0) {
		commands.push_back(
			{.kind = UIDrawKind::Line, .color = color, .thickness = thickness, .p0 = a, .p1 = b, .order = order}
		);
	}

	void PushCircle(const glm::vec2 center, const float radius, const core::Rgba color, const int order = 0) {
		commands.push_back(
			{.kind = UIDrawKind::Circle, .color = color, .p0 = center, .radius = radius, .order = order}
		);
	}

	// Queue a textured rect: `texture` (a LoadTexture handle) tiled into `r`, modulated by `tint`.
	void PushTexturedRect(const int texture, const core::Rect r, const core::Rgba tint, const int order = 0) {
		commands.push_back(
			{.kind = UIDrawKind::TexturedRect, .rect = r, .color = tint, .texture = texture, .order = order}
		);
	}

	void PushBeginClip(const core::Rect r, const int order = 0) {
		commands.push_back({.kind = UIDrawKind::BeginClip, .rect = r, .order = order});
	}

	void PushEndClip(const int order = 0) { commands.push_back({.kind = UIDrawKind::EndClip, .order = order}); }
};

} // namespace engine::ui
