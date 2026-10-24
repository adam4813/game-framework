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

// ── Crab (prototype values; the enemy-type registry replaces these with ticket 09) ──
inline constexpr float kCrabHp = 3.0F;
inline constexpr float kCrabSpeed = 4.2F;
inline constexpr float kCrabRadius = 0.9F;
inline constexpr float kCrabContactDps = 10.0F;
// Enemy HP grows over the run: hp * (1 + clock / kEnemyHpGrowthSeconds).
inline constexpr float kEnemyHpGrowthSeconds = 240.0F;
// Each frame a landed enemy moves out of this fraction of its overlap with its neighbours.
inline constexpr float kSeparationRelax = 0.5F;
// Spatial grid cell size for crowding and hit checks.
inline constexpr float kCrowdCellSize = 2.2F;

// ── Crabnado ──
inline constexpr float kCrabnadoSpeed = 6.0F;
// Waypoints are picked inside this fraction of the arena radius, so the crabnado never parks at the edge.
inline constexpr float kCrabnadoRoamFraction = 0.6F;
inline constexpr float kCrabnadoArriveRadius = 2.0F;
// Spawns per second: kSpawnRateBase + kSpawnRateGrowth * clock seconds.
inline constexpr float kSpawnRateBase = 1.6F;
inline constexpr float kSpawnRateGrowth = 0.09F;
// Throw: the landing point is 6–32 units from the crabnado toward the player, ±10 units of jitter.
inline constexpr float kThrowMinDistance = 6.0F;
inline constexpr float kThrowMaxDistance = 32.0F;
inline constexpr float kThrowJitter = 10.0F;
inline constexpr float kThrowHeight = 16.0F;
inline constexpr float kThrowMinTime = 1.1F;
inline constexpr float kThrowMaxTime = 1.6F;
inline constexpr float kGravity = 30.0F;

// ── Player ──
inline constexpr float kPlayerSpeed = 11.0F;
inline constexpr float kPlayerMaxHp = 100.0F;
// Enemies touch the player within their radius plus this.
inline constexpr float kPlayerContactRadius = 0.9F;

// ── Claw snap ──
inline constexpr float kClawDamage = 2.0F;
inline constexpr float kClawRadius = 4.5F;
inline constexpr float kClawCooldown = 0.9F;
// How long the claw ring and an enemy's hit flash stay visible.
inline constexpr float kClawRingTime = 0.2F;
inline constexpr float kHitFlashTime = 0.08F;

// ── Camera ──
// North-up angled view that follows the player.
inline constexpr float kCameraHeight = 38.0F;
inline constexpr float kCameraBack = 30.0F;

// ── Debugging ──
// Flecs REST port; differs from the sandbox's 27750 so both can run side by side.
inline constexpr int kRestPort = 27751;

} // namespace crabnado::constants
