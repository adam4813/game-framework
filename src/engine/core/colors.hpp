#pragma once

#include "core_types.hpp"

namespace engine::core::colors {

// Predefined color palette for UI and game elements.
inline constexpr Rgba Background{.r = 24, .g = 28, .b = 24, .a = 255};
inline constexpr Rgba Panel{.r = 40, .g = 46, .b = 40, .a = 255};
inline constexpr Rgba PanelHi{.r = 56, .g = 64, .b = 56, .a = 255};
inline constexpr Rgba PanelBg{.r = 18, .g = 20, .b = 18, .a = 235};
inline constexpr Rgba Border{.r = 90, .g = 100, .b = 90, .a = 255};
inline constexpr Rgba Accent{.r = 70, .g = 140, .b = 70, .a = 255};
inline constexpr Rgba Title{.r = 120, .g = 200, .b = 120, .a = 255};
inline constexpr Rgba Text{.r = 220, .g = 225, .b = 220, .a = 255};
inline constexpr Rgba Subtle{.r = 140, .g = 150, .b = 140, .a = 255};
inline constexpr Rgba Pollinator{.r = 170, .g = 120, .b = 210, .a = 255};
inline constexpr Rgba Water{.r = 70, .g = 130, .b = 200, .a = 255};
inline constexpr Rgba Fertilizer{.r = 90, .g = 170, .b = 90, .a = 255};
inline constexpr Rgba Boost{.r = 220, .g = 150, .b = 60, .a = 255};
inline constexpr Rgba Hazard{.r = 200, .g = 70, .b = 70, .a = 255};

} // namespace engine::core::colors
