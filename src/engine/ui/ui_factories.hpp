#pragma once

#include <functional>
#include <string>
#include <vector>

#include <flecs.h>

#include "ui_components.hpp"

// Convenience factories for the data-driven 2D UI. Each returns a fresh entity carrying a UIRect
// plus the relevant visual component(s). Parent the returned entity to another UI entity
// (child_entity.child_of(parent)) to compose hierarchies and control draw order.
namespace engine::ui {

/// Register a path as the default click sound for all buttons created by CreateButton.
/// Call once at game startup, before any scenes load. Does nothing if path is empty.
void SetDefaultClickSound(const flecs::world& world, std::string path);

/// Create a background panel occupying `rect`. Override styling via the optional `panel`.
flecs::entity CreatePanel(const flecs::world& world, core::Rect rect, Panel panel = {});

/// Create a static text label occupying `rect`.
flecs::entity CreateLabel(const flecs::world& world, core::Rect rect, std::string text, float font_size = 20.0F);

/// Create a clickable button occupying `rect`. When clicked it invokes `on_click` (if set) and
/// fires an audio::SoundEffect component, if one is attached to the returned entity. Pass a
/// `texture` handle (from platform LoadTexture) to render a tiled textured background instead of a
/// flat colour.
flecs::entity CreateButton(
	const flecs::world& world,
	core::Rect rect,
	std::string label,
	std::function<void(flecs::entity)> on_click = {},
	int texture = -1
);

/// Create a determinate progress bar occupying `rect`. Drive it via `entity.get_mut<ProgressBar>()`.
flecs::entity CreateProgressBar(const flecs::world& world, core::Rect rect, const ProgressBar& bar = {});

/// Create an interactive horizontal slider occupying `rect`. `on_change` is called whenever the
/// value is modified by the user; capture context in the lambda.
flecs::entity CreateSlider(
	const flecs::world& world,
	core::Rect rect,
	float value,
	float min,
	float max,
	std::function<void(float)> on_change = {},
	float step = 0.0F
);

/// Create an indeterminate loading spinner occupying `rect` (its centre is the ring centre).
flecs::entity CreateSpinner(const flecs::world& world, core::Rect rect, const Spinner& spinner = {});

/// Create an auto-layout container. Parent UI entities to it (via `.child_of`) to have their
/// positions arranged along the stack direction each frame.
flecs::entity CreateStack(const flecs::world& world, core::Rect rect, const Stack& stack = {});

/// Create a scrollable viewport occupying `rect`. Parent content (often a Stack) to it; it is
/// clipped to `rect` and scrolled with the mouse wheel. A background Panel is added by default.
flecs::entity CreateScrollRect(const flecs::world& world, core::Rect rect, const ScrollRect& scroll = {});

/// Create a modal dialog: a centred Panel of size `rect` over a full-screen backdrop, rendered
/// above all other UI. Parent content to the returned entity. Toggle via `entity.get_mut<Modal>()`.
flecs::entity CreateModal(const flecs::world& world, core::Rect rect, const Modal& modal = {});

/// Create a checkbox occupying `rect`, with `label` drawn to the right of the box. `on_change` is
/// called with the new checked state when toggled.
flecs::entity CreateCheckbox(
	const flecs::world& world,
	core::Rect rect,
	std::string label,
	bool checked = false,
	std::function<void(bool)> on_change = {}
);

/// Create a radio button occupying `rect`. Radios sharing the same `group` are mutually exclusive;
/// `value` identifies this option and is passed to `on_change` when it becomes selected.
flecs::entity CreateRadio(
	const flecs::world& world,
	core::Rect rect,
	std::string group,
	std::string value,
	std::string label,
	std::function<void(const std::string&)> on_change = {}
);

/// Create a single-line text input occupying `rect`. `on_change` fires on every edit; bind
/// `TextInput::on_submit` for Enter handling. Click to focus; typed text appends while focused.
flecs::entity CreateTextInput(
	const flecs::world& world,
	core::Rect rect,
	std::string placeholder = {},
	std::function<void(const std::string&)> on_change = {}
);

/// Create a multi-line text area occupying `rect`. Enter inserts a newline. `on_change` fires on
/// every edit.
flecs::entity CreateTextArea(
	const flecs::world& world,
	core::Rect rect,
	std::string placeholder = {},
	std::function<void(const std::string&)> on_change = {}
);

/// Create a drop-down selector occupying `rect`, populated with `options`. `on_change` is called
/// with the selected index when the user picks an option.
flecs::entity CreateDropdown(
	const flecs::world& world,
	core::Rect rect,
	std::vector<std::string> options,
	std::function<void(int)> on_change = {}
);

// One button of a prompt: its label and the handler to run when pressed. The handler receives the
// clicked button entity (use `e.world()` to reach the world, e.g. to remove a state tag).
struct PromptButton {
	std::string label;
	std::function<void(flecs::entity)> on_click;
};

/// Create a prompt dialog composed from a Modal + a vertical Stack containing a title Label, a
/// message Label, and a horizontal row of Buttons. Returns the modal entity. Demonstrates building
/// a compound widget purely from the primitive UI components.
flecs::entity CreatePrompt(
	const flecs::world& world,
	core::Rect rect,
	std::string title,
	std::string message,
	std::vector<PromptButton> buttons
);

} // namespace engine::ui
