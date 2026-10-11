#pragma once

#include <flecs.h>

namespace engine::ui {

// Register engine::level loaders for the 2D UI components so a UI tree can be authored directly in a
// `.level.json` file (entity tree + "components"/"children"/"refs"), rather than only in C++.
// Component keys: ui_rect, ui_element, panel, label, button, progress_bar, slider, spinner, stack,
// scroll_rect, modal, checkbox, radio, text_input, text_area, dropdown, data_binding. The data
// binding's source entity is wired with the "binding_source" ref (resolved in the level's second
// pass). Called by UIModule; LevelModule must be imported first.
void RegisterUILevelLoaders(const flecs::world& world);

} // namespace engine::ui
