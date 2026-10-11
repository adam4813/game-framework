#pragma once

#include <flecs.h>

namespace engine::ui {

// Register the UI interaction/layout/build/render systems with Flecs. Called by UIModule after it
// sets up the singletons, reflection, and scripting bindings. Split from the module so the module
// constructor stays focused on registration wiring and the systems (with their shared internal
// helpers) live in one translation unit.
void RegisterUISystems(const flecs::world& world);

} // namespace engine::ui
