#pragma once

#include <functional>

#include <flecs.h>

// Behaviour/event components for the data-driven 2D UI. These carry std::function callbacks (not
// plain data), so they live apart from ui_components.hpp and are intentionally engine-only (not
// script-reflected as value types — scripts bind handlers through the module's callback-function
// helpers such as SetOnClick instead).
namespace engine::ui {

// Event handler invoked once when a Button on the same entity registers a completed click. The
// clicked entity is passed so one shared callback can service many buttons.
struct OnClick {
	std::function<void(flecs::entity)> callback;
};

} // namespace engine::ui
