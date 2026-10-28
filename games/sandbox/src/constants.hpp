#pragma once

#include <string_view>

// Shared tuning constants. All values live here as constexpr so balance changes
// never require touching gameplay logic. Values sourced from the prototype spec.
namespace app::constants {

// ── Window ──
inline constexpr int kWindowWidth = 1280;
inline constexpr int kWindowHeight = 720;
inline constexpr std::string_view kWindowTitle = "Game";

} // namespace app::constants
