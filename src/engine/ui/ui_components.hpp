#pragma once

#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "engine/core/core.hpp"

// Data-driven 2D UI. An entity becomes a UI element by pairing a UIRect (its position + size)
// with one or more visual components (Panel, Label, Button). Behaviour is attached as extra
// components on the same entity (OnClick handler from ui_events.hpp, audio::SoundEffect, ...), so
// a "button" is simply the composition UIRect + Button + OnClick + SoundEffect — no bespoke widget
// classes.
//
// Rendering is deferred (see ui_draw_list.hpp): the visual systems never call the platform
// directly. They append draw commands to the UIDrawList singleton, then a single flush system
// issues the platform draw calls in order. Draw order is FIFO, refined by the entity hierarchy (a
// child composites on top of its parent) and an optional UIElement.z_index layer.
//
// This header holds only plain-data components (and pure-data singletons/tags). The deferred-render
// types live in ui_draw_list.hpp, callback/behaviour components in ui_events.hpp, the Create*
// factories in ui_factories.hpp, and the systems in ui_systems.hpp.
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

// Global UI configuration singleton. Set once at game startup via ui::SetDefaultClickSound or by
// setting this singleton directly. The CreateButton factory reads it to auto-attach a SoundEffect.
struct UIConfig {
	std::string default_click_sound_path; // empty = no default sound
};

// Cached per-frame UI input state singleton, recomputed at the very start of the interaction phase
// by UIUpdateModalState. `any_open` lets button/widget hit-tests cheaply honour modal input-blocking
// without querying all modals. `pointer_consumed` is a short-term "stop propagation" flag: a widget
// on top (a dismissing modal backdrop, an open dropdown's list) sets it when it handles the press,
// and every widget's interactability check skips once it is set — so the click isn't also delivered
// to whatever sits beneath. (A full per-event capture/bubble model would supersede this.)
struct UIModalState {
	bool any_open{false};
	bool pointer_consumed{false};
};

// Keyboard-focus singleton: the UI entity currently receiving typed text (a focused text field), or
// an invalid entity when nothing is focused. Text-input systems apply InputState.text_input and the
// editing keys only to the focused element; clicking a field moves focus, clicking elsewhere clears
// it. Holds an entity handle — validate with `focused.is_alive()` before use, since the focused
// element may be destroyed on scene teardown.
struct UIFocus {
	flecs::entity focused;
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

// Toggleable boolean control: a small box that shows a check mark when `checked`, with an optional
// `label` drawn to its right. UICheckboxInteraction toggles `checked` on click and fires `on_change`
// with the new state. Radio is the grouped, mutually-exclusive variant.
struct Checkbox {
	bool checked{false};
	std::string label;
	float font_size{18.0F};
	float box_size{22.0F};
	core::Rgba box_color{core::colors::PanelHi};
	core::Rgba check_color{core::colors::Accent};
	core::Rgba border_color{core::colors::Border};
	core::Rgba text_color{core::colors::Text};
	float roundness{0.2F};

	// === Runtime state (written by UICheckboxInteraction) ===
	bool hovered{false};

	// Called with the new checked state when toggled via interaction.
	std::function<void(bool)> on_change;
};

// Grouped, mutually-exclusive toggle (a "radio button"): all Radios sharing the same non-empty
// `group` string form one set — selecting one (`selected = true`) deselects the others.
// UIRadioInteraction renders a circle with a filled dot when selected and an optional `label`. When
// clicked it selects this option, clears the rest of the group, and fires `on_change` with `value`.
struct Radio {
	std::string group; // radios with the same group are mutually exclusive
	std::string value; // identifies this option (passed to on_change)
	bool selected{false};
	std::string label;
	float font_size{18.0F};
	float radius{11.0F};
	core::Rgba circle_color{core::colors::PanelHi};
	core::Rgba dot_color{core::colors::Accent};
	core::Rgba border_color{core::colors::Border};
	core::Rgba text_color{core::colors::Text};

	// === Runtime state (written by UIRadioInteraction) ===
	bool hovered{false};

	// Called with this radio's `value` when it becomes selected via interaction.
	std::function<void(const std::string&)> on_change;
};

// Single-line editable text field. Click to focus (tracked by the UIFocus singleton); while focused
// it consumes InputState.text_input (appending to `text`) and Backspace (held-to-repeat), and Enter
// fires `on_submit`. `on_change` fires whenever the text changes. Shows dimmed `placeholder` when
// empty, and a blinking caret while focused. The displayed text is clipped to the field.
struct TextInput {
	std::string text;
	std::string placeholder;
	int max_length{0}; // 0 = unlimited
	float font_size{18.0F};
	core::Rgba bg_color{core::colors::PanelBg};
	core::Rgba border_color{core::colors::Border};
	core::Rgba focus_border_color{core::colors::Accent};
	core::Rgba text_color{core::colors::Text};
	core::Rgba placeholder_color{core::colors::Subtle};
	core::Rgba caret_color{core::colors::Text};
	float roundness{0.15F};

	// === Runtime state (written by the text-input systems) ===
	bool hovered{false};
	float caret_blink{0.0F}; // advances while focused; drives the caret blink

	// Called with the current text whenever it changes via editing.
	std::function<void(const std::string&)> on_change;
	// Called with the current text when Enter is pressed while focused.
	std::function<void(const std::string&)> on_submit;
};

// Multi-line editable text field. Behaves like TextInput, but Enter inserts a newline (the text is
// word-wrapped to the field width) rather than submitting. Click to focus; consumes typed text and
// Backspace while focused; shows a blinking caret.
struct TextArea {
	std::string text;
	std::string placeholder;
	int max_length{0}; // 0 = unlimited
	float font_size{18.0F};
	float line_spacing{4.0F};
	core::Rgba bg_color{core::colors::PanelBg};
	core::Rgba border_color{core::colors::Border};
	core::Rgba focus_border_color{core::colors::Accent};
	core::Rgba text_color{core::colors::Text};
	core::Rgba placeholder_color{core::colors::Subtle};
	core::Rgba caret_color{core::colors::Text};
	float roundness{0.08F};

	// === Runtime state (written by the text-input systems) ===
	bool hovered{false};
	float caret_blink{0.0F};

	// Called with the current text whenever it changes via editing.
	std::function<void(const std::string&)> on_change;
};

// Drop-down selector: a closed box showing the selected option (or `placeholder`); clicking toggles
// `open`, revealing the option list below. UIDropdownInteraction sets `selected` (an index into
// `options`) on choosing an option, closes the list, and fires `on_change`. The open list renders
// above sibling UI and closes on selection or an outside click.
struct Dropdown {
	std::vector<std::string> options;
	int selected{-1}; // index into options; -1 = none selected
	std::string placeholder{"Select..."};
	bool open{false};
	float font_size{18.0F};
	float row_height{28.0F};
	core::Rgba bg_color{core::colors::PanelBg};
	core::Rgba border_color{core::colors::Border};
	core::Rgba text_color{core::colors::Text};
	core::Rgba panel_color{core::colors::Panel};
	core::Rgba hover_color{core::colors::PanelHi};
	float roundness{0.15F};

	// === Runtime state (written by UIDropdownInteraction) ===
	bool hovered{false};
	int hover_index{-1}; // option row under the cursor while open, or -1

	// Called with the newly selected index when an option is chosen.
	std::function<void(int)> on_change;

	// Text shown in the closed box: the selected option, or the placeholder when nothing is selected.
	[[nodiscard]] const std::string& DisplayText() const {
		if (selected >= 0 && static_cast<size_t>(selected) < options.size()) {
			return options[static_cast<size_t>(selected)];
		}
		return placeholder;
	}
};

} // namespace engine::ui
