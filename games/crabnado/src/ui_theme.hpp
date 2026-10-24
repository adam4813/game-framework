#pragma once

#include <flecs.h>

#include "engine/engine.hpp"

// Crabnado's menu look: a chunky display font, sand-coloured panels with the title's navy outline, and coral buttons.
namespace crabnado {

// Text colour for labels drawn on a panel (and the default for every label the UI factories create).
inline constexpr engine::core::Rgba kPanelTextColor{.r = 18, .g = 40, .b = 70, .a = 255};
// The button colour, also used to mark a selected molt card.
inline constexpr engine::core::Rgba kCoralColor{.r = 224, .g = 94, .b = 58, .a = 255};
// A stat change for the better and for the worse, on a panel.
inline constexpr engine::core::Rgba kGainColor{.r = 30, .g = 140, .b = 70, .a = 255};
inline constexpr engine::core::Rgba kLossColor{.r = 200, .g = 50, .b = 35, .a = 255};

// Loads the UI font and sets the engine's UIConfig theme. Call before any scene builds its UI.
void ApplyUiTheme(const flecs::world& world);

} // namespace crabnado
