#pragma once

#include <functional>
#include <string>
#include <vector>

#include <flecs.h>

#include "engine/platform/platform.hpp"
#include "ui_components.hpp"

namespace engine::ui {

/// UI module for Flecs ECS integration.
/// Registers the 2D UI components, the UIDrawList singleton, and the interaction/build/render
/// systems that run in the engine::ecs::OnUI phase. Systems are untagged (they run in every
/// scene pipeline) and are not Pausable, so UI keeps working while the game is paused.
class UIModule {
public:
	explicit UIModule(const flecs::world& world);
};

// === Convenience factories ===
// Each returns a fresh entity carrying a UIRect plus the relevant visual component. Parent the
// returned entity to another UI entity (child_entity.child_of(parent)) to compose hierarchies and
// control draw order.

/// Register a path as the default click sound for all buttons created by CreateButton.
/// Call once at game startup, before any scenes load. Does nothing if path is empty.
void SetDefaultClickSound(const flecs::world& world, std::string path);

/// Create a background panel occupying `rect`. Override styling via the optional `panel`.
flecs::entity CreatePanel(const flecs::world& world, platform::Rect rect, Panel panel = {});

/// Create a static text label occupying `rect`.
flecs::entity CreateLabel(const flecs::world& world, platform::Rect rect, std::string text, float font_size = 20.0F);

/// Create a clickable button occupying `rect`. When clicked it invokes `on_click` (if set) and
/// fires an audio::SoundEffect component, if one is attached to the returned entity.
flecs::entity CreateButton(
	const flecs::world& world,
	platform::Rect rect,
	std::string label,
	std::function<void(flecs::entity)> on_click = {}
);

/// Create a determinate progress bar occupying `rect`. Drive it via `entity.get_mut<ProgressBar>()`.
flecs::entity CreateProgressBar(const flecs::world& world, platform::Rect rect, const ProgressBar& bar = {});

/// Create an indeterminate loading spinner occupying `rect` (its centre is the ring centre).
flecs::entity CreateSpinner(const flecs::world& world, platform::Rect rect, const Spinner& spinner = {});

/// Create an auto-layout container. Parent UI entities to it (via `.child_of`) to have their
/// positions arranged along the stack direction each frame.
flecs::entity CreateStack(const flecs::world& world, platform::Rect rect, const Stack& stack = {});

/// Create a scrollable viewport occupying `rect`. Parent content (often a Stack) to it; it is
/// clipped to `rect` and scrolled with the mouse wheel. A background Panel is added by default.
flecs::entity CreateScrollRect(const flecs::world& world, platform::Rect rect, const ScrollRect& scroll = {});

/// Create a modal dialog: a centred Panel of size `rect` over a full-screen backdrop, rendered
/// above all other UI. Parent content to the returned entity. Toggle via `entity.get_mut<Modal>()`.
flecs::entity CreateModal(const flecs::world& world, platform::Rect rect, const Modal& modal = {});

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
	platform::Rect rect,
	std::string title,
	std::string message,
	std::vector<PromptButton> buttons
);

} // namespace engine::ui
