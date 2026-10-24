#pragma once

#include <string_view>

// Crabnado tuning constants. Balance values join these as the spec's systems land.
namespace crabnado::constants {

// ── Window ──
inline constexpr int kWindowWidth = 1280;
inline constexpr int kWindowHeight = 720;
inline constexpr std::string_view kWindowTitle = "Crabnado";

// ── UI ──
// Lilita One (SIL Open Font License, assets/fonts/crabnado/OFL.txt).
inline constexpr std::string_view kUiFontPath = "fonts/crabnado/LilitaOne-Regular.ttf";

// ── Audio ──
inline constexpr float kMusicVolume = 0.5F;
// The title music is a little quieter. When a run is won the arena music ducks to kEndedMusicVolume so the sting is
// heard; when it is lost the arena music stops and the game-over track plays.
inline constexpr float kTitleMusicVolume = 0.4F;
inline constexpr float kEndedMusicVolume = 0.15F;
inline constexpr float kGameOverMusicVolume = 0.5F;

// ── Horde ──
// Most enemies alive at once, from ticket 01's desktop performance check (about 60 fps at 1,000).
inline constexpr int kHordeCap = 1000;
// The debug spawn control adds this many crabs per press, up to kHordeDebugMax.
inline constexpr int kHordeDebugBatch = 100;
inline constexpr int kHordeDebugMax = 1000;
inline constexpr float kArenaRadius = 70.0F;

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
// Base 1.6 -> 1.0 and growth 0.09 -> 0.035 (ticket 15): scripted circling runs died at 0:70-0:100 on the old curve.
inline constexpr float kSpawnRateBase = 1.0F;
inline constexpr float kSpawnRateGrowth = 0.035F;
// No throws for this many seconds at the start of a run (ticket 15).
inline constexpr float kFirstThrowDelay = 3.0F;
// Throw: the landing point is 6–32 units from the crabnado toward the player, ±10 units of jitter.
inline constexpr float kThrowMinDistance = 6.0F;
inline constexpr float kThrowMaxDistance = 32.0F;
inline constexpr float kThrowJitter = 10.0F;
inline constexpr float kThrowHeight = 16.0F;
inline constexpr float kThrowMinTime = 1.1F;
inline constexpr float kThrowMaxTime = 1.6F;
inline constexpr float kGravity = 30.0F;

// ── Wind (prototype values) ──
// The direction shifts every kWindShiftMin–kWindShiftMax seconds by kWindTurnMin–kWindTurnMax radians
// (45–90°), turning at kWindTurnRate radians per second. The HUD warns kWindWarningTime seconds ahead.
inline constexpr float kWindShiftMin = 20.0F;
inline constexpr float kWindShiftMax = 30.0F;
inline constexpr float kWindTurnMin = 0.785F;
inline constexpr float kWindTurnMax = 1.571F;
inline constexpr float kWindTurnRate = 0.35F;
inline constexpr float kWindWarningTime = 3.0F;
// Strength is kWindBaseStrength ± kWindSwell (a slow sine) plus the gust, which jumps to kWindGust every
// kWindGustMin–kWindGustMax seconds and fades at kWindGustFade per second.
inline constexpr float kWindBaseStrength = 0.8F;
inline constexpr float kWindSwell = 0.2F;
inline constexpr float kWindGust = 0.45F;
inline constexpr float kWindGustMin = 6.0F;
inline constexpr float kWindGustMax = 14.0F;
inline constexpr float kWindGustFade = 0.25F;
// Wind push per unit of strength, in units per second. Waves, not wind, are the main force.
inline constexpr float kPlayerWindPush = 1.5F;
inline constexpr float kEnemyWindPush = 1.2F;
// The crabnado's next waypoint shifts this far downwind per unit of strength.
inline constexpr float kCrabnadoWindBias = 14.0F;

// ── Waves (prototype values) ──
// Crests travel at kWaveSpeed units per second, kWavePeriod seconds apart, and are Gaussian bands of
// half-width kWaveWidth. Every third crest is kWaveBigAmp times higher. Push is the crest's height times
// kWavePushK, along its travel.
inline constexpr float kWaveSpeed = 11.0F;
inline constexpr float kWavePeriod = 6.0F;
inline constexpr float kWaveWidth = 3.4F;
inline constexpr float kWaveBigAmp = 2.2F;
inline constexpr float kWavePushK = 9.0F;
// The ramp to chaos, in run-clock seconds: from kCrossSwellAt a wind shift adds a cross-swell and the old swells
// fade (amp falls by dt / kSwellFadeTime; a new swell grows over kSwellGrowTime and shows its notice for
// kCrossSwellNotice seconds). From kRogueAt rogue waves arrive: the first after kRogueFirstMin..Max seconds, then
// every kRogueGapMin..Max. A rogue's bearing is the wind's +/- kRogueSpread, its amp kRogueAmp and its width
// kWaveWidth * sqrt(kRogueWidthK). It reaches the player kRogueLead seconds after it spawns, is warned of
// kRogueWarning seconds ahead, and is removed once past the arena by kRogueExitMargin.
inline constexpr float kCrossSwellAt = 90.0F;
inline constexpr float kRogueAt = 180.0F;
inline constexpr float kSwellFadeTime = 45.0F;
inline constexpr float kSwellGrowTime = 4.0F;
inline constexpr float kCrossSwellNotice = 2.0F;
inline constexpr float kRogueFirstMin = 6.0F;
inline constexpr float kRogueFirstMax = 10.0F;
inline constexpr float kRogueGapMin = 12.0F;
inline constexpr float kRogueGapMax = 20.0F;
inline constexpr float kRogueSpread = 0.6F;
inline constexpr float kRogueAmp = 2.6F;
inline constexpr float kRogueWidthK = 1.6F;
inline constexpr float kRogueLead = 2.5F;
inline constexpr float kRogueWarning = 2.6F;
inline constexpr float kRogueExitMargin = 30.0F;
// A tide pool calms waves by up to this fraction at its centre, over the outer kTidePoolCalmEdge of its radius.
inline constexpr float kTidePoolCalm = 0.92F;
inline constexpr float kTidePoolCalmEdge = 0.4F;
// The water grid: tiles of kWaterCell, filled to kWaterFill, covering the arena plus kWaterMargin. A tile's top
// sits at the wave height times kWaterHeightScale; the foam tint ramps up between kFoamMinHeight and
// kFoamMinHeight + kFoamRamp.
inline constexpr float kWaterCell = 2.6F;
inline constexpr float kWaterFill = 0.97F;
inline constexpr float kWaterMargin = 2.0F;
inline constexpr float kWaterHeightScale = 0.75F;
inline constexpr float kWaterTileThickness = 1.0F;
inline constexpr float kSandY = -0.6F;
inline constexpr float kFoamMinHeight = 0.35F;
inline constexpr float kFoamRamp = 1.3F;
// The HUD warns of a big crest this many seconds before it reaches the player.
inline constexpr float kBigCrestWarning = 2.2F;
// Shells drift with this fraction of the wave push until they fly to the player.
inline constexpr float kShellWavePush = 0.4F;
// Enemies over their type's tumble_min wave height are tumbled for kTumbleTime seconds, spinning at kTumbleSpin radians per
// second. Their knock decays by exp(-kKnockDecay * dt) once the crest has passed.
inline constexpr float kTumbleTime = 0.9F;
inline constexpr float kTumbleSpin = 9.0F;
inline constexpr float kKnockDecay = 3.0F;

// A tumbling enemy knocked into a rock faster than kSlamMinSpeed takes (speed - kSlamSpeedOffset) * kSlamDamage
// and flashes for kSlamFlashTime; its knock is then multiplied by kSlamBounce.
inline constexpr float kSlamMinSpeed = 5.0F;
inline constexpr float kSlamSpeedOffset = 4.0F;
inline constexpr float kSlamDamage = 1.6F;
inline constexpr float kSlamFlashTime = 0.2F;
inline constexpr float kSlamBounce = -0.2F;

// ── Boss (prototype values) ──
// At kBossAt seconds the crabnado comes ashore and chases the player at kBossSpeed. A wave higher than
// kBossKnockMinHeight carries it at kBossKnockPush of the wave push, else its knock decays by exp(-kKnockDecay * dt).
// It is a circle of kBossRadius against rocks and stays kBossEdgeMargin inside the arena edge, and within
// kBossContactRange of the player it drains kBossContactDps HP per second. Knocked into a rock faster than
// kBossSlamMinSpeed it takes a slam (kBossSlamCooldown seconds apart; knock * kBossSlamBounce); kBossSlamsToWin win.
// Spawns run at kBossSpawnFactor of the normal rate. Banners show for kBossBannerTime seconds.
inline constexpr float kBossAt = 300.0F;
inline constexpr float kBossSpeed = 5.0F;
inline constexpr float kBossKnockMinHeight = 1.3F;
inline constexpr float kBossKnockPush = 0.9F;
inline constexpr float kBossRadius = 3.0F;
inline constexpr float kBossEdgeMargin = 8.0F;
inline constexpr float kBossContactRange = 4.0F;
inline constexpr float kBossContactDps = 25.0F;
inline constexpr float kBossSlamMinSpeed = 6.0F;
inline constexpr float kBossSlamCooldown = 2.5F;
inline constexpr float kBossSlamBounce = -0.3F;
inline constexpr int kBossSlamsToWin = 3;
inline constexpr float kBossSpawnFactor = 0.35F;
inline constexpr float kBossBannerTime = 3.0F;

// ── Player ──
inline constexpr float kPlayerSpeed = 11.0F;
inline constexpr float kPlayerMaxHp = 100.0F;
// Surfing: a crest higher than kSurfMinHeight carries the player at its push; the knock decays by
// exp(-kKnockDecay * dt) afterwards and counts as surfing above kSurfingSpeed units per second.
inline constexpr float kSurfMinHeight = 0.3F;
inline constexpr float kSurfingSpeed = 4.0F;
// Digging in squashes the crab's y scale to kDigInSquash and sinks it by kDigInSink.
inline constexpr float kDigInSquash = 0.7F;
inline constexpr float kDigInSink = 0.35F;
// Stamina (units, units per second). Once it runs out, dig in stays off until stamina refills to
// kStaminaRecoverFraction of the maximum.
inline constexpr float kStaminaMax = 100.0F;
inline constexpr float kStaminaRegen = 18.0F;
inline constexpr float kStaminaDrain = 30.0F;
inline constexpr float kStaminaRecoverFraction = 0.25F;
// Enemies touch the player within their radius plus this.
inline constexpr float kPlayerContactRadius = 0.9F;

// Shells inside this range fly to the player.
inline constexpr float kPlayerPickupRange = 5.0F;

// ── Shells and molting ──
inline constexpr float kShellFlySpeed = 25.0F;
// A flying shell is collected within this distance of the player.
inline constexpr float kShellCollectRadius = 1.0F;
// Shells needed per molt (first molt 6 -> 18, ticket 15): round(kShellsFirstMolt * kShellsGrowth^(level - 1)). Geometric because the kill rate rises
// with the spawn rate; the old linear 6 + 5 per molt gave a 4th molt at 0:18 (ticket 15).
inline constexpr int kShellsFirstMolt = 18;
inline constexpr float kShellsGrowth = 1.3F;
// Upgrade registry, relative to the asset directory.
inline constexpr std::string_view kEnemiesPath = "/data/crabnado/enemies.json";
inline constexpr std::string_view kUpgradesPath = "/data/crabnado/upgrades.json";

// ── Claw snap ──
inline constexpr float kClawDamage = 2.0F;
inline constexpr float kClawRadius = 4.5F;
inline constexpr float kClawCooldown = 0.9F;
// How long the claw ring and an enemy's hit flash stay visible.
inline constexpr float kClawRingTime = 0.2F;
inline constexpr float kHitFlashTime = 0.08F;

// ── Bubbles (prototype values) ──
// Fired at the nearest landed enemy within kBubbleRange, kBubbleSpread radians apart. In flight a bubble gains
// the wind push times kBubbleWindDrift plus the wave push times kBubbleWaveDrift, per second.

// Riptide: unlocked by an upgrade. Every kRiptideCooldown seconds it multiplies the next crest of the primary swell
// that is at least kRiptideLead seconds away by kRiptideMultiplier. The first fire comes kRiptideFirstDelay s after
// the unlock.
inline constexpr float kRiptideCooldown = 8.0F;
inline constexpr float kRiptideMultiplier = 2.2F;
inline constexpr float kRiptideLead = 1.0F;
inline constexpr float kRiptideFirstDelay = 2.0F;
inline constexpr float kBubbleDamage = 2.0F;
inline constexpr float kBubbleCooldown = 0.7F;
inline constexpr float kBubbleSpeed = 22.0F;
inline constexpr float kBubbleRange = 30.0F;
inline constexpr float kBubbleSpread = 0.18F;
inline constexpr float kBubbleLife = 1.6F;
inline constexpr float kBubbleRadius = 0.35F;
inline constexpr int kBubbleCap = 400;
inline constexpr float kBubbleWindDrift = 12.0F;
inline constexpr float kBubbleWaveDrift = 1.5F;

// ── Camera ──
// North-up angled view that follows the player.
inline constexpr float kCameraHeight = 38.0F;
inline constexpr float kCameraBack = 30.0F;

// ── Debugging ──
// Flecs REST port; differs from the sandbox's 27750 so both can run side by side.
inline constexpr int kRestPort = 27751;

} // namespace crabnado::constants
