#pragma once

#include "engine/core/core.hpp"

namespace signal_room {

// Shift the game opens on (asset-relative).
inline constexpr const char* kBootShift = "shifts/shift_01.shift.json";
// Directory the Room panel lists Shift files from (asset-relative).
inline constexpr const char* kShiftDirectory = "shifts";

// Bare-floor greybox until the palette file arrives (ticket 02): a grout slab under inset cell tiles.
inline constexpr float kFloorThickness = 0.1F;
inline constexpr float kFloorTileInset = 0.04F;
inline constexpr engine::core::Rgba kFloorGroutColour{.r = 58, .g = 56, .b = 52, .a = 255};
inline constexpr engine::core::Rgba kFloorTileColour{.r = 132, .g = 126, .b = 112, .a = 255};
inline constexpr engine::core::Rgba kFloorTileAltColour{.r = 120, .g = 114, .b = 101, .a = 255};

} // namespace signal_room
