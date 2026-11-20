#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include <flecs.h>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"

// The campaign: `campaign.json` holds Acts, each with inline Stages and a Boss Stage. The run walks them: a Stage's
// gate ends it, its spawn table and rate drive the crabnado's throws, and the Act's clocks drive enemy HP and the sea.
// Loaded on every arena scene Load, so an edit shows on the next Play. A run is in an Act, in a Stage of it
// (`stage == stages.size()` is the Boss Stage); see RunState.
namespace crabnado {

// How a Stage ends: `Time` at `time` seconds on the Stage clock, `Quota` at `kills` defeats in the Stage, `Both` at
// whichever comes first (reaching the quota in time pays the bonus), `Script` only when the Stage's script calls
// EndStage() (see campaign_script.hpp; a Stage whose script died ends at kScriptGateFallbackTime instead).
enum class GateType { Time, Quota, Both, Script };

// What a `both` gate pays for meeting its quota in time: shells (through the same path a pickup uses, so it can
// trigger a molt), a molt reroll, or a heal in HP. A missing `amount` means half the shells the molt needs, one
// reroll, or 30% of max HP.
enum class BonusType { Shells, Reroll, Heal };

struct GateBonus {
	BonusType type{BonusType::Shells};
	std::optional<float> amount;
};

struct Gate {
	GateType type{GateType::Time};
	float time{0.0F};
	int kills{0};
	GateBonus bonus;
};

// One entry of a spawn table: an enemy type (an index into the EnemyRegistry), its relative weight, and the id of the
// spawner that brings it in.
struct SpawnEntry {
	int type{0};
	float weight{1.0F};
	std::string source;
};

// Spawns per second on the Stage clock: min(max, base + growth * stage_clock). No `max` means no cap.
struct RateCurve {
	float base{1.0F};
	float growth{0.0F};
	std::optional<float> max;
};

// Enemy HP is type.hp * mult * (1 + act_clock / growth_seconds); growth_seconds 0 means no growth.
struct HpScale {
	float mult{1.0F};
	float growth_seconds{0.0F};
};

struct Stage {
	std::string name;
	Gate gate;
	std::vector<SpawnEntry> spawns;
	RateCurve rate;
	// Spawner for entries that name none.
	std::string source;
	// Replaces the Act's HpScale as a whole; still runs on the Act clock.
	std::optional<HpScale> hp_scale;
	// The id of this Stage's script (assets/scripts/crabnado/<id>.as), loaded while the Stage runs. A `script` gate needs one.
	std::string script;
};

struct BossStage {
	std::string id;
	std::vector<SpawnEntry> spawns;
	// The Boss Stage's rate is the last Stage's rate as its gate ends, times this. It doesn't grow.
	float rate_mult{1.0F};
};

// The act's arena: rock counts that override the level's rock_scatter bands, and a tint every water tile's base color
// lerps toward by `amount`.
struct ActArena {
	std::optional<int> rocks_inner;
	std::optional<int> rocks_outer;
	engine::core::Rgba tint_color{.r = 255, .g = 255, .b = 255, .a = 255};
	float tint_amount{0.0F};
};

// Seconds on the Act clock at which the sea escalates. A missing key means that event never happens in this Act.
struct ActSea {
	std::optional<float> cross_swell_at;
	std::optional<float> rogue_at;
};

// The `offscreen` spawner's tunables, from an Act's "offscreen" block: volleys of volley_min-volley_max enemies thrown
// from one point `radius` units from the centre, each volley's bearing at least `min_turn_deg` degrees from the last.
struct OffscreenSpawner {
	int volley_min{3};
	int volley_max{6};
	float radius{85.0F};
	float min_turn_deg{60.0F};
};

struct Act {
	std::string id;
	std::string name;
	std::string title;
	// False hides the crabnado through the Act's normal Stages (it returns for the Boss Stage); its spawner `crabnado` is
	// then a bad reference in those Stages.
	bool crabnado{true};
	// Present when the Act authored an "offscreen" block; the spawner `offscreen` needs it.
	std::optional<OffscreenSpawner> offscreen;
	ActArena arena;
	ActSea sea;
	HpScale hp_scale;
	std::vector<Stage> stages;
	// Absent when the Act has none or its boss id didn't resolve; the Act then clears when its last Stage gates.
	std::optional<BossStage> boss;
	// The id of the comic page shown after this Act (the last Act's is the ending). Ticket 06 makes the pages real.
	std::string comic_after;
	// The id of this Act's script, loaded for the Act and keeping its state through its Stages.
	std::string script;
};

// Singleton: the loaded campaign.
struct Campaign {
	std::vector<Act> acts;
};

// Singleton, not reflected: the (act, stage) pair the director last applied. When RunState's reflected pair differs
// (the Remote API wrote it), the director enters that Stage.
struct DirectorApplied {
	int act{-1};
	int stage{-1};
};

// Singleton, reflected: where the arena loads its campaign, bosses and scripts from. A relative path resolves against the
// asset directory; an absolute one is used as is, so a test can point the arena at a fixture. `scripts_path` is a
// directory ending in '/', holding `<id>.as`. Set it before the arena scene loads.
struct CampaignSource {
	std::string campaign_path{constants::kCampaignPath};
	std::string bosses_path{constants::kBossesPath};
	std::string scripts_path{constants::kScriptsDir};
};

// `path` as a CampaignSource names it: used as is when absolute, else relative to `assetDirectory`.
[[nodiscard]] std::string ResolveSourcePath(std::string_view assetDirectory, const std::string& path);

struct CampaignParseResult {
	Campaign campaign;
	// One line per dropped entry or bad document, ready to log.
	std::vector<std::string> errors;
};

// Parses a campaign.json document. `enemyIds` are the enemy type ids in EnemyRegistry order (an entry's `type` is its
// position), `bossIds` the registered bosses and `spawnerIds` the known spawners. An unknown enemy type or spawner
// drops that spawn entry (an unknown Stage source falls back to "crabnado"), and an unknown boss id drops the Boss
// Stage, so a typo can never soft-lock a run. A script id `scriptExists` rejects is dropped from its Act or Stage, and a
// `script` gate with no usable script drops the Stage; no `scriptExists` means script ids aren't checked. Every case is
// reported in `errors`. Pure, so tests call it without the engine.
[[nodiscard]] CampaignParseResult ParseCampaign(
	const nlohmann::json& doc,
	std::span<const std::string> enemyIds,
	std::span<const std::string> bossIds,
	std::span<const std::string_view> spawnerIds,
	const std::function<bool(std::string_view)>& scriptExists = {}
);

// The text of a Stage's challenge banner line, "Defeat 40 in 1:00!". Empty unless the gate is `both`.
[[nodiscard]] std::string ChallengeText(const Gate& gate);

// "m:ss" for a number of seconds, rounded up (a clock counting down).
[[nodiscard]] std::string FormatClock(float seconds);

// The spawner ids the code registers. Ticket 10 adds `infested_crest`.
[[nodiscard]] std::span<const std::string_view> SpawnerIds();

// Loads bosses.json then campaign.json (the CampaignSource paths, resolved against `assetDirectory`), replacing the BossRegistry and
// Campaign singletons and logging every error. Call after LoadEnemyTypes.
void LoadCampaignData(const flecs::world& world, std::string_view assetDirectory);

// Spawns per second `rate` gives at `stageClock`.
[[nodiscard]] float SpawnRate(const RateCurve& rate, float stageClock);

// The Boss Stage's spawn rate: the Act's last Stage's rate as its gate ends (its time, for a time gate), times
// `rate_mult`. 0 for an Act with no Stages or no boss.
[[nodiscard]] float BossSpawnRate(const Act& act);

// HP multiplier at `actClock`: the Stage's hp_scale if it has one, else the Act's. `stage` may be null (the Boss Stage).
[[nodiscard]] float EnemyHpScale(const Act& act, const Stage* stage, float actClock);

// The Act the run is in, or nullptr when RunState::act is out of range.
[[nodiscard]] const Act* CurrentAct(const flecs::world& world);

// The normal Stage the run is in, or nullptr in the Boss Stage (or out of range).
[[nodiscard]] const Stage* CurrentStage(const flecs::world& world);

// Whether the run is in its Act's Boss Stage.
[[nodiscard]] bool InBossStage(const flecs::world& world);

// What the crabnado throws now: the current spawn table and rate. nullopt when there is no Act or Stage to read.
struct SpawnPlan {
	const std::vector<SpawnEntry>* spawns;
	float rate;
};
[[nodiscard]] std::optional<SpawnPlan> CurrentSpawnPlan(const flecs::world& world);

// The HP multiplier enemies spawned now get.
[[nodiscard]] float CurrentEnemyHpScale(const flecs::world& world);

// Ends the current Stage early, as a script's EndStage() does: the Stage's `both` bonus is paid only if its quota is met.
// Returns why it was refused, or an empty string when it ended the Stage: refused in a Boss Stage, a molt, a Transition
// or when the run isn't playing.
[[nodiscard]] std::string_view EndStageEarly(const flecs::world& world);

// Brings the Act's boss ashore now, skipping the rest of the storm-up, as a script's SpawnBoss(id) does. Returns why it
// was refused, or an empty string when it did: refused unless this is the Act's Boss Stage, `bossId` is its boss and the
// boss isn't up yet.
[[nodiscard]] std::string_view CallBossNow(const flecs::world& world, std::string_view bossId);

// Shows or hides the crabnado (an Act with `"crabnado": false` has none outside its Boss Stage). Called as a Stage begins.
void SetCrabnadoPresent(const flecs::world& world, bool present);

// Registers and reflects nothing but pre-registers the campaign singletons, and adds the CampaignDirector system.
// Call from the module constructor after RegisterRun.
void RegisterCampaign(const flecs::world& world);

} // namespace crabnado
