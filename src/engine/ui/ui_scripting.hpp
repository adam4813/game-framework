#pragma once

#include <flecs.h>

namespace engine::ui {

// Expose the Create* UI factories to scripts as global functions that return the created entity, so
// scripts can build UI (not just read it). Each takes a `parent` Entity (the new widget is parented
// to it when valid) plus the rect and widget-specific arguments, e.g.:
//   Entity panel = CreatePanel(self.GetParent(), 20, 20, 240, 160);
//   Entity ok    = CreateButton(panel, 0, 0, 120, 40, "OK");
// Called by UIModule after the scripting backend is initialised.
void RegisterUIScriptFactories(const flecs::world& world);

} // namespace engine::ui
