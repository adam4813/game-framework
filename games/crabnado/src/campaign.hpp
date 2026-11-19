#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include <flecs.h>

#include "engine/engine.hpp"

// The campaign: `campaign.json` holds Acts, each with inline Stages and a Boss Stage. The run walks them: a Stage's
// gate ends it, its spawn table and rate drive the crabnado's throws, and the Act's clocks drive enemy HP and the sea.
// Loaded on every arena scene Load, so an edit shows on the next Play. A run is in an Act, in a Stage of it
// (`stage == stages.size()` is the Boss Stage); see RunState.
namespace crabnado {

// How a Stage ends. Only `Time` is evaluated so far; ticket 03 adds the others (the parser accepts them now).
enum class GateType { Time, Quota, Both, Script };

struct Gate {
	GateType type{GateType::Time};
	float time{0.0F};
	int kills{0};
	float bonus{0.0F};
	std::string script;
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

struct Act {
	std::string id;
	std::string name;
	std::string title;
	ActArena arena;
	ActSea sea;
	HpScale hp_scale;
	std::vector<Stage> stages;
	// Absent when the Act has none or its boss id didn't resolve; the Act then clears when its last Stage gates.
	std::optional<BossStage> boss;
};

// Singleton: the loaded campaign.
struct Campaign {
	std::vector<Act> acts;
};

// Singleton, not reflected: the (act, stage) pair the director last applied. When RunState's reflected pair differs
// (the Remote API wrote it), the director enters that Stage.
struct DirectorApplied {
	int act{0};
	int stage{0};
};

struct CampaignParseResult {
	Campaign campaign;
	// One line per dropped entry or bad document, ready to log.
	std::vector<std::string> errors;
};

// Parses a campaign.json document. `enemyIds` are the enemy type ids in EnemyRegistry order (an entry's `type` is its
// position), `bossIds` the registered bosses and `spawnerIds` the known spawners. An unknown enemy type or spawner
// drops that spawn entry (an unknown Stage source falls back to "crabnado"), and an unknown boss id drops the Boss
// Stage, so a typo can never soft-lock a run. Every case is reported in `errors`. Pure, so tests call it without the
// engine.
[[nodiscard]] CampaignParseResult ParseCampaign(
	const nlohmann::json& doc,
	std::span<const std::string> enemyIds,
	std::span<const std::string> bossIds,
	std::span<const std::string_view> spawnerIds
);

// The spawner ids the code registers. Tickets 09 and 10 add `offscreen` and `infested_crest`.
[[nodiscard]] std::span<const std::string_view> SpawnerIds();

// Loads bosses.json then campaign.json (both resolved against `assetDirectory`), replacing the BossRegistry and
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

// Registers and reflects nothing but pre-registers the campaign singletons, and adds the CampaignDirector system.
// Call from the module constructor after RegisterRun.
void RegisterCampaign(const flecs::world& world);

} // namespace crabnado
