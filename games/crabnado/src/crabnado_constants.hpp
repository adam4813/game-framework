#pragma once

#include <string_view>

// Crabnado tuning constants. Balance values join these as the spec's systems land.
namespace crabnado::constants {

// ── Window ──
inline constexpr int kWindowWidth = 1280;
inline constexpr int kWindowHeight = 720;
inline constexpr std::string_view kWindowTitle = "Crabnado";

// ── Horde ──
// Most enemies alive at once, from ticket 01's desktop performance check (about 60 fps at 1,000).
inline constexpr int kHordeCap = 1000;
// The debug spawn control adds this many crabs per press, up to kHordeDebugMax.
inline constexpr int kHordeDebugBatch = 100;
inline constexpr int kHordeDebugMax = 1000;
inline constexpr float kArenaRadius = 70.0F;
inline constexpr float kCrabSpeed = 4.2F;
// Crabs stop walking once this close to their goal.
inline constexpr float kCrabArriveRadius = 2.0F;

// ── Debugging ──
// Flecs REST port; differs from the sandbox's 27750 so both can run side by side.
inline constexpr int kRestPort = 27751;

} // namespace crabnado::constants
