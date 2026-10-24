#pragma once

#include <string_view>

// Crabnado tuning constants. Balance values join these as the spec's systems land.
namespace crabnado::constants {

// ── Window ──
inline constexpr int kWindowWidth = 1280;
inline constexpr int kWindowHeight = 720;
inline constexpr std::string_view kWindowTitle = "Crabnado";

// ── Debugging ──
// Flecs REST port; differs from the sandbox's 27750 so both can run side by side.
inline constexpr int kRestPort = 27751;

} // namespace crabnado::constants
