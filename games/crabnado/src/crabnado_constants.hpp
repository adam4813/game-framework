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

// ── Run start ──
// A run opens with a 3-2-1 countdown of kCountdownTime seconds, the scene paused, then "Go!" shows for kGoTime.
inline constexpr float kCountdownTime = 3.0F;
inline constexpr float kGoTime = 0.7F;
// After a molt, play resumes after kResumeTime seconds of "Ready...", then the same "Go!".
inline constexpr float kResumeTime = 1.0F;
// Between Acts, "ACT CLEAR" shows for kActClearTime seconds before the comic page.
inline constexpr float kActClearTime = 2.5F;

// Scattered rocks keep this clear of the player's start (the centre).
inline constexpr float kStartClearance = 5.0F;

// ── Horde ──
// Most enemies alive at once, from ticket 01's desktop performance check (about 60 fps at 1,000).
inline constexpr int kHordeCap = 1000;
// The debug spawn control adds this many crabs per press, up to kHordeDebugMax.
inline constexpr int kHordeDebugBatch = 100;
inline constexpr int kHordeDebugMax = 1000;
inline constexpr float kArenaRadius = 70.0F;

// Each frame a landed enemy moves out of this fraction of its overlap with its neighbours.
inline constexpr float kSeparationRelax = 0.5F;
// Spatial grid cell size for crowding and hit checks.
inline constexpr float kCrowdCellSize = 2.2F;

// ── Crabnado ──
// The orbit's radius, speed and gust response are the `crabnado` entry's in bosses.json. The crabnado spawns at
// kOrbitFallbackRadius until the first orbit tick reads them. Its start bearing is at least kOrbitStartMinAngle
// (90 degrees) from the player's.
inline constexpr float kOrbitFallbackRadius = 78.0F;
inline constexpr float kOrbitStartMinAngle = 1.5708F;
// The spawn tables and rates are the Stages' in campaign.json. No throws for this many seconds of the Act (ticket 15).
inline constexpr float kFirstThrowDelay = 3.0F;
// The storm-up's last kStormHoldTime seconds hold the crabnado still at the orbit point nearest the player; before
// that it slides there.
inline constexpr float kStormHoldTime = 2.0F;
// The breach: the crabnado moves from the orbit to the arena edge over kBreachTime seconds, and back out over
// kRetreatTime once beaten. Barrier pieces within kBarrierFlareArc radians (20 degrees) of its bearing flare.
inline constexpr float kBreachTime = 1.5F;
inline constexpr float kRetreatTime = 2.0F;
inline constexpr float kBarrierFlareArc = 0.349F;
// Orbit throw: the landing point is kLandingMin-kLandingMax units in from the barrier on the line toward the player,
// never past the player, with kThrowJitter units of jitter. The Boss Stage's body throw lands kThrowMinDistance-
// kThrowMaxDistance units from the crabnado toward the player instead.
inline constexpr float kLandingMin = 12.0F;
inline constexpr float kLandingMax = 45.0F;
inline constexpr float kThrowMinDistance = 6.0F;
inline constexpr float kThrowMaxDistance = 32.0F;
inline constexpr float kThrowJitter = 10.0F;
inline constexpr float kThrowHeight = 16.0F;
inline constexpr float kThrowMinTime = 1.5F;
inline constexpr float kThrowMaxTime = 2.2F;
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
// How many wind lines are drawn, as a share of the full count: the arena's streaks and the title's breeze
// lines. 1 is the original density.
inline constexpr float kWindLineDensity = 0.25F;

// ── Waves (prototype values) ──
// Crests travel at kWaveSpeed units per second, kWavePeriod seconds apart, and are Gaussian bands of
// half-width kWaveWidth. Every third crest is kWaveBigAmp times higher. Push is the crest's height times
// kWavePushK, along its travel.
inline constexpr float kWaveSpeed = 11.0F;
inline constexpr float kWavePeriod = 6.0F;
inline constexpr float kWaveWidth = 3.4F;
inline constexpr float kWaveBigAmp = 2.2F;
inline constexpr float kWavePushK = 9.0F;
// The ramp to chaos, in Act-clock seconds (the Act's `sea` times in campaign.json): from `cross_swell_at` a wind shift
// adds a cross-swell and the old swells
// fade (amp falls by dt / kSwellFadeTime; a new swell grows over kSwellGrowTime and shows its notice for
// kCrossSwellNotice seconds). From `rogue_at` rogue waves arrive: the first after kRogueFirstMin..Max seconds, then
// every kRogueGapMin..Max. A rogue's bearing is the wind's +/- kRogueSpread, its amp kRogueAmp and its width
// kWaveWidth * sqrt(kRogueWidthK). It rises at the upwind edge of the open ocean (kOceanFarReach), rolls in across the
// arena, is warned of kRogueWarning seconds before it reaches the player, and is removed at the downwind edge.
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
inline constexpr float kRogueWarning = 2.6F;
// A tide pool calms waves by up to this fraction at its centre, over the outer kTidePoolCalmEdge of its radius.
inline constexpr float kTidePoolCalm = 0.92F;
inline constexpr float kTidePoolCalmEdge = 0.4F;
// The water grid: tiles of kWaterCell, filled to kWaterFill, covering the arena plus kWaterMargin. Past that the
// ocean runs on to the horizon in coarser tiles: kWaterCell * 2 out to kOceanNearReach, kWaterCell * 4 out to
// kOceanFarReach. A tile's top sits at the wave height times kWaterHeightScale; the foam tint ramps up between
// kFoamMinHeight and kFoamMinHeight + kFoamRamp. The seabed, a quad kSeabedReach from the centre at kSeabedY,
// fills the gaps and everything past the farthest tiles.
inline constexpr float kWaterCell = 2.6F;
inline constexpr float kWaterFill = 0.97F;
inline constexpr float kWaterMargin = 2.0F;
inline constexpr float kOceanNearReach = 100.0F;
inline constexpr float kOceanFarReach = 150.0F;
inline constexpr float kWaterHeightScale = 0.75F;
inline constexpr float kWaterTileThickness = 1.0F;
inline constexpr float kSeabedY = -0.6F;
inline constexpr float kSeabedReach = 400.0F;
inline constexpr float kFoamMinHeight = 0.35F;
inline constexpr float kFoamRamp = 1.3F;
// The HUD warns of a big crest this many seconds before it reaches the player.
inline constexpr float kBigCrestWarning = 2.2F;
// Shells drift with this fraction of the wave push until they fly to the player.
inline constexpr float kShellWavePush = 0.4F;
// A crest over an enemy type's tumble_min grabs it and carries it (a ride) for kRideBase + kRidePerHeight * (peak - tumble_min)
// seconds, at most kRideMax, at kRidePush of the crest's push. Afterwards no crest can grab it for kImmune seconds (it
// still gets kImmunePush of the push). It then winds up for kWindup seconds and charges the player's position at
// kChargeMult times its speed, decaying linearly to 1x over kChargeTime seconds, dealing kChargeDpsMult times its contact
// damage. Each enemy type can override these in enemies.json. The ride spins at kTumbleSpin radians per second and its
// knock decays by exp(-kKnockDecay * dt) once the crest has passed.
inline constexpr float kRideBase = 0.5F;
inline constexpr float kRidePerHeight = 0.6F;
inline constexpr float kRideMax = 2.0F;
inline constexpr float kRidePush = 1.0F;
inline constexpr float kImmune = 1.5F;
inline constexpr float kImmunePush = 0.0F;
inline constexpr float kWindup = 0.35F;
inline constexpr float kChargeMult = 2.2F;
inline constexpr float kChargeTime = 1.2F;
inline constexpr float kChargeDpsMult = 1.5F;
inline constexpr float kWindupCrouch = 0.15F;
inline constexpr float kWindupShiver = 0.25F;
inline constexpr float kWindupShiverRate = 60.0F;
inline constexpr float kTumbleSpin = 9.0F;
inline constexpr float kKnockDecay = 3.0F;

// A tumbling enemy knocked into a rock faster than kSlamMinSpeed takes (speed - kSlamSpeedOffset) * kSlamDamage
// and flashes for kSlamFlashTime; its knock is then multiplied by kSlamBounce.
inline constexpr float kSlamMinSpeed = 5.0F;
inline constexpr float kSlamSpeedOffset = 4.0F;
inline constexpr float kSlamDamage = 1.6F;
inline constexpr float kSlamFlashTime = 0.2F;
inline constexpr float kSlamBounce = -0.2F;

// ── Boss (presentation) ──
// The boss's behavior tunables live in bosses.json. Banners show for kBossBannerTime seconds.
inline constexpr float kBossBannerTime = 3.0F;
// Every Stage change shows its banner for kStageBannerTime seconds, and "Challenge cleared!" shows for
// kChallengeClearedTime.
inline constexpr float kStageBannerTime = 2.0F;
inline constexpr float kChallengeClearedTime = 2.0F;
// The storm wind-up: when the last normal Stage gates, the crabnado stops throwing for kStormUpTime seconds while it
// darkens to storm grey and spins and sways harder, then the boss lands and it stays that way. At full storm it spins kStormSpin times faster, and its
// rings sway kStormSwayRate times faster and kStormSwayReach times wider. Each slam flashes it pale, fading over
// 1 / kStormFlashFade seconds.
inline constexpr float kStormUpTime = 5.0F;
inline constexpr float kStormSpin = 2.0F;
inline constexpr float kStormSwayRate = 1.8F;
inline constexpr float kStormSwayReach = 1.6F;
inline constexpr float kStormFlashFade = 2.5F;
// The enraged crabnado: its funnel strobes pale between 0 and 1 at kWindupStrobeHz while it winds up an attack and holds
// at kDazedFlash while dazed. A volley disc is as wide as the landing crab's radius plus kVolleyDiscMargin.
inline constexpr float kWindupStrobeHz = 8.0F;
inline constexpr float kDazedFlash = 0.6F;
inline constexpr float kVolleyDiscMargin = 0.9F;

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
// The campaign (Acts, Stages) and the boss registry, relative to the asset directory.
inline constexpr std::string_view kCampaignPath = "/data/crabnado/campaign.json";
inline constexpr std::string_view kBossesPath = "/data/crabnado/bosses.json";
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
