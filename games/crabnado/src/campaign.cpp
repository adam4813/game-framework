#include "campaign.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <functional>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "boss_registry.hpp"
#include "campaign_script.hpp"
#include "crabnado.hpp"
#include "crabnado_constants.hpp"
#include "enemy_types.hpp"
#include "player.hpp"
#include "run.hpp"
#include "sfx.hpp"

using namespace engine;

namespace crabnado {

namespace {

using json = nlohmann::json;

constexpr std::string_view kLogTag = "Campaign";
constexpr std::string_view kDefaultSource = "crabnado";
constexpr std::string_view kOffscreenSource = "offscreen";
constexpr std::string_view kInfestedSource = "infested_crest";
constexpr std::array<std::string_view, 3> kSpawnerIds{"crabnado", kOffscreenSource, kInfestedSource};

// What the parsers share. A fatal problem in a Stage or Act throws std::runtime_error, caught where that Stage or Act
// is dropped; a droppable one (a bad spawn entry) goes straight into `errors`.
struct Context {
	std::span<const std::string> enemyIds;
	std::span<const std::string> bossIds;
	std::span<const std::string_view> spawnerIds;
	const std::function<bool(std::string_view)>& scriptExists;
	std::vector<std::string>& errors;
};

bool Contains(const std::span<const std::string_view> ids, const std::string_view id) {
	return std::ranges::find(ids, id) != ids.end();
}

float Number(const json& j, const char* key, const float fallback) {
	if (!j.contains(key)) {
		return fallback;
	}
	if (!j[key].is_number()) {
		throw std::runtime_error(std::format("'{}' must be a number", key));
	}
	return j[key].get<float>();
}

std::optional<float> OptionalNumber(const json& j, const char* key) {
	if (!j.contains(key)) {
		return std::nullopt;
	}
	return Number(j, key, 0.0F);
}

std::string Text(const json& j, const char* key) {
	return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string{};
}

const json& Object(const json& j, const char* key) {
	static const json kEmpty = json::object();
	if (!j.contains(key)) {
		return kEmpty;
	}
	if (!j[key].is_object()) {
		throw std::runtime_error(std::format("'{}' must be an object", key));
	}
	return j[key];
}

GateBonus ParseBonus(const json& j) {
	if (!j.is_object()) {
		throw std::runtime_error("'bonus' must be an object {type, amount}");
	}
	const std::string type = Text(j, "type");
	GateBonus bonus;
	if (type == "shells") {
		bonus.type = BonusType::Shells;
	} else if (type == "reroll") {
		bonus.type = BonusType::Reroll;
	} else if (type == "heal") {
		bonus.type = BonusType::Heal;
	} else {
		throw std::runtime_error(std::format("unknown bonus type '{}'", type));
	}
	bonus.amount = OptionalNumber(j, "amount");
	return bonus;
}

Gate ParseGate(const json& j) {
	if (!j.is_object()) {
		throw std::runtime_error("gate must be an object");
	}
	const std::string type = Text(j, "type");
	Gate gate;
	if (type == "time") {
		gate.type = GateType::Time;
		if (!j.contains("time")) {
			throw std::runtime_error("a time gate needs 'time'");
		}
	} else if (type == "quota") {
		gate.type = GateType::Quota;
		if (!j.contains("kills")) {
			throw std::runtime_error("a quota gate needs 'kills'");
		}
	} else if (type == "both") {
		gate.type = GateType::Both;
		if (!j.contains("time") || !j.contains("kills") || !j.contains("bonus")) {
			throw std::runtime_error("a both gate needs 'time', 'kills' and 'bonus'");
		}
		gate.bonus = ParseBonus(j["bonus"]);
	} else if (type == "script") {
		gate.type = GateType::Script;
	} else {
		throw std::runtime_error(std::format("unknown gate type '{}'", type));
	}
	gate.time = Number(j, "time", 0.0F);
	gate.kills = static_cast<int>(Number(j, "kills", 0.0F));
	return gate;
}

// The script id under `key`, empty when there is none or `scriptExists` rejects it (reported in `errors`).
std::string ParseScriptId(const json& j, const char* key, const std::string& where, Context& ctx) {
	std::string id = Text(j, key);
	if (!id.empty() && ctx.scriptExists && !ctx.scriptExists(id)) {
		ctx.errors.push_back(std::format("{}: unknown script '{}', script skipped", where, id));
		return {};
	}
	return id;
}

// Reads an infested_crest entry's `chance`, `riders` and `min_gap` into `entry`; false when one is malformed.
bool ParseInfested(const json& value, SpawnEntry& entry) {
	const auto number = [&](const char* key, const float fallback) -> std::optional<float> {
		if (!value.contains(key)) {
			return fallback;
		}
		return value[key].is_number() ? std::optional{value[key].get<float>()} : std::nullopt;
	};
	const auto chance = number("chance", entry.chance);
	const auto gap = number("min_gap", entry.min_gap);
	if (!chance || !gap || *chance < 0.0F || *chance > 1.0F || *gap < 0.0F) {
		return false;
	}
	entry.chance = *chance;
	entry.min_gap = *gap;
	if (value.contains("riders")) {
		const auto& riders = value["riders"];
		if (!riders.is_array() || riders.size() != 2 || !riders[0].is_number_integer() || !riders[1].is_number_integer()) {
			return false;
		}
		entry.riders_min = riders[0].get<int>();
		entry.riders_max = riders[1].get<int>();
		return entry.riders_min >= 1 && entry.riders_max >= entry.riders_min;
	}
	return true;
}

// A spawn table: enemy id to a weight, or to {weight, source}. An unknown enemy type or spawner drops that entry.
std::vector<SpawnEntry> ParseSpawns(const json& spawns, const std::string_view defaultSource, const std::string& where, Context& ctx) {
	std::vector<SpawnEntry> table;
	for (const auto& [enemy, value] : spawns.items()) {
		const auto known = std::ranges::find(ctx.enemyIds, enemy);
		if (known == ctx.enemyIds.end()) {
			ctx.errors.push_back(std::format("{}: unknown enemy type '{}', entry skipped", where, enemy));
			continue;
		}
		SpawnEntry entry{.type = static_cast<int>(known - ctx.enemyIds.begin()), .weight = 0.0F, .source = std::string{defaultSource}};
		if (value.is_number()) {
			entry.weight = value.get<float>();
		} else if (value.is_object() && Text(value, "source") == kInfestedSource) {
			// No weight: an infested crest is rolled per crest, never picked from the table.
			entry.source = kInfestedSource;
			entry.weight = 0.0F;
			if (!ParseInfested(value, entry)) {
				ctx.errors.push_back(std::format(
					"{}: infested_crest '{}' needs a numeric 'chance' (0-1), 'riders' [min, max] and 'min_gap', entry skipped", where, enemy
				));
				continue;
			}
		} else if (value.is_object() && value.contains("weight") && value["weight"].is_number()) {
			entry.weight = value["weight"].get<float>();
			if (value.contains("source")) {
				entry.source = Text(value, "source");
			}
		} else {
			ctx.errors.push_back(std::format("{}: spawn '{}' must be a weight or {{weight, source}}, entry skipped", where, enemy));
			continue;
		}
		if (!Contains(ctx.spawnerIds, entry.source)) {
			ctx.errors.push_back(std::format("{}: unknown spawner '{}' for '{}', entry skipped", where, entry.source, enemy));
			continue;
		}
		table.push_back(std::move(entry));
	}
	return table;
}

HpScale ParseHpScale(const json& j) {
	return {.mult = Number(j, "mult", 1.0F), .growth_seconds = Number(j, "growth_seconds", 0.0F)};
}

RateCurve ParseRate(const json& j) {
	return {.base = Number(j, "base", 1.0F), .growth = Number(j, "growth", 0.0F), .max = OptionalNumber(j, "max")};
}

Stage ParseStage(const json& j, const std::string& where, Context& ctx) {
	if (!j.is_object()) {
		throw std::runtime_error("must be an object");
	}
	Stage stage;
	stage.name = Text(j, "name");
	stage.gate = ParseGate(j.value("gate", json{}));
	stage.script = ParseScriptId(j, "script", where, ctx);
	// A `script` gate may also carry the script id, so `{"type": "script", "script": "x"}` reads naturally.
	if (stage.script.empty() && stage.gate.type == GateType::Script) {
		stage.script = ParseScriptId(j.value("gate", json{}), "script", where, ctx);
	}
	if (stage.gate.type == GateType::Script && stage.script.empty()) {
		throw std::runtime_error("a script gate needs a 'script'");
	}
	stage.source = Text(j, "source");
	if (stage.source.empty()) {
		stage.source = kDefaultSource;
	} else if (!Contains(ctx.spawnerIds, stage.source)) {
		ctx.errors.push_back(std::format("{}: unknown spawner '{}', using '{}'", where, stage.source, kDefaultSource));
		stage.source = kDefaultSource;
	}
	stage.spawns = ParseSpawns(Object(j, "spawns"), stage.source, where, ctx);
	stage.rate = ParseRate(Object(j, "rate"));
	if (j.contains("hp_scale")) {
		stage.hp_scale = ParseHpScale(Object(j, "hp_scale"));
	}
	return stage;
}

core::Rgba ParseTintColor(const json& j) {
	core::Rgba color{.r = 255, .g = 255, .b = 255, .a = 255};
	if (!j.contains("color")) {
		return color;
	}
	const json& c = j["color"];
	if (!c.is_array() || c.size() < 3 || !std::ranges::all_of(c, [](const json& v) { return v.is_number(); })) {
		throw std::runtime_error("tint 'color' must be [r, g, b]");
	}
	color.r = c[0].get<uint8_t>();
	color.g = c[1].get<uint8_t>();
	color.b = c[2].get<uint8_t>();
	return color;
}

ActArena ParseArena(const json& j) {
	ActArena arena;
	const json& rocks = Object(j, "rocks");
	if (const auto inner = OptionalNumber(rocks, "inner")) {
		arena.rocks_inner = static_cast<int>(*inner);
	}
	if (const auto outer = OptionalNumber(rocks, "outer")) {
		arena.rocks_outer = static_cast<int>(*outer);
	}
	const json& tint = Object(j, "tint");
	arena.tint_color = ParseTintColor(tint);
	arena.tint_amount = Number(tint, "amount", 0.0F);
	return arena;
}

OffscreenSpawner ParseOffscreen(const json& j) {
	const OffscreenSpawner defaults;
	OffscreenSpawner spawner{
		.volley_min = static_cast<int>(Number(j, "volley_min", static_cast<float>(defaults.volley_min))),
		.volley_max = static_cast<int>(Number(j, "volley_max", static_cast<float>(defaults.volley_max))),
		.radius = Number(j, "radius", defaults.radius),
		.min_turn_deg = Number(j, "min_turn_deg", defaults.min_turn_deg),
	};
	if (spawner.volley_min < 1 || spawner.volley_max < spawner.volley_min) {
		throw std::runtime_error("needs 1 <= volley_min <= volley_max");
	}
	if (spawner.radius <= constants::kArenaRadius) {
		throw std::runtime_error(std::format("'radius' must be beyond the arena edge ({})", constants::kArenaRadius));
	}
	if (spawner.min_turn_deg < 0.0F || spawner.min_turn_deg > 180.0F) {
		throw std::runtime_error("'min_turn_deg' must be 0-180");
	}
	return spawner;
}

// Drops the spawn entries whose spawner the Act can't supply, and says so.
void DropBadSpawners(std::vector<SpawnEntry>& spawns, const Act& act, const bool normalStage, const std::string& where, Context& ctx) {
	std::erase_if(spawns, [&](const SpawnEntry& entry) {
		const bool noBlock = entry.source == kOffscreenSource && !act.offscreen;
		const bool noCrabnado = entry.source == kDefaultSource && normalStage && !act.crabnado;
		if (noBlock || noCrabnado) {
			ctx.errors.push_back(std::format(
				"{}: spawner '{}' has no {} in this Act, entry skipped", where, entry.source,
				noBlock ? "\"offscreen\" block" : "crabnado (\"crabnado\": false)"
			));
		}
		return noBlock || noCrabnado;
	});
}

BossStage ParseBossStage(const json& j, const std::string& where, Context& ctx) {
	BossStage boss;
	boss.id = Text(j, "id");
	boss.spawns = ParseSpawns(Object(j, "spawns"), kDefaultSource, where, ctx);
	boss.rate_mult = Number(j, "rate_mult", 1.0F);
	return boss;
}

Act ParseAct(const json& j, const std::string& where, Context& ctx) {
	if (!j.is_object()) {
		throw std::runtime_error("must be an object");
	}
	Act act;
	act.id = Text(j, "id");
	if (act.id.empty()) {
		throw std::runtime_error("missing 'id'");
	}
	act.name = Text(j, "name");
	act.title = Text(j, "title");
	act.comic_after = Text(j, "comic_after");
	act.script = ParseScriptId(j, "script", where, ctx);
	act.arena = ParseArena(Object(j, "arena"));
	const json& sea = Object(j, "sea");
	act.sea = {.cross_swell_at = OptionalNumber(sea, "cross_swell_at"), .rogue_at = OptionalNumber(sea, "rogue_at")};
	act.hp_scale = ParseHpScale(Object(j, "hp_scale"));
	if (j.contains("crabnado")) {
		if (!j["crabnado"].is_boolean()) {
			throw std::runtime_error("'crabnado' must be true or false");
		}
		act.crabnado = j["crabnado"].get<bool>();
	}
	if (j.contains("offscreen")) {
		try {
			act.offscreen = ParseOffscreen(Object(j, "offscreen"));
		} catch (const std::exception& ex) {
			ctx.errors.push_back(std::format("{} offscreen: {}, block skipped", where, ex.what()));
		}
	}

	if (j.contains("stages")) {
		if (!j["stages"].is_array()) {
			throw std::runtime_error("'stages' must be an array");
		}
		int index = 0;
		for (const json& entry : j["stages"]) {
			const std::string stageWhere = std::format("{} stage {}", where, index++);
			try {
				Stage stage = ParseStage(entry, stageWhere, ctx);
				DropBadSpawners(stage.spawns, act, true, stageWhere, ctx);
				act.stages.push_back(std::move(stage));
			} catch (const std::exception& ex) {
				ctx.errors.push_back(std::format("{}: {}, stage skipped", stageWhere, ex.what()));
			}
		}
	}
	if (j.contains("boss")) {
		const std::string bossWhere = std::format("{} boss", where);
		try {
			BossStage boss = ParseBossStage(Object(j, "boss"), bossWhere, ctx);
			DropBadSpawners(boss.spawns, act, false, bossWhere, ctx);
			if (std::ranges::find(ctx.bossIds, boss.id) == ctx.bossIds.end()) {
				ctx.errors.push_back(std::format("{}: unknown boss id '{}', the Act clears when its last Stage gates", bossWhere, boss.id));
			} else {
				act.boss = std::move(boss);
			}
		} catch (const std::exception& ex) {
			ctx.errors.push_back(std::format("{}: {}, boss skipped", bossWhere, ex.what()));
		}
	}
	if (act.stages.empty() && !act.boss) {
		throw std::runtime_error("has no Stages and no boss");
	}
	return act;
}

} // namespace

CampaignParseResult ParseCampaign(
	const json& doc,
	const std::span<const std::string> enemyIds,
	const std::span<const std::string> bossIds,
	const std::span<const std::string_view> spawnerIds,
	const std::function<bool(std::string_view)>& scriptExists
) {
	CampaignParseResult result;
	Context ctx{enemyIds, bossIds, spawnerIds, scriptExists, result.errors};
	if (!doc.is_object() || !doc.contains("acts") || !doc["acts"].is_array()) {
		result.errors.emplace_back("campaign.json must be an object with an 'acts' array");
		return result;
	}
	int index = 0;
	for (const json& entry : doc["acts"]) {
		const std::string where = std::format("act {}", index++);
		try {
			result.campaign.acts.push_back(ParseAct(entry, where, ctx));
		} catch (const std::exception& ex) {
			result.errors.push_back(std::format("{}: {}, act skipped", where, ex.what()));
		}
	}
	if (result.campaign.acts.empty()) {
		result.errors.emplace_back("campaign has no valid Acts");
	}
	return result;
}

std::span<const std::string_view> SpawnerIds() {
	return kSpawnerIds;
}

void SetCrabnadoPresent(const flecs::world& world, const bool present) {
	// A disabled entity is skipped by every system and render query, its funnel (its children) with it.
	if (present) {
		world.query_builder().with<Crabnado>().with(flecs::Disabled).build().each([](const flecs::entity crabnado) { crabnado.enable(); });
		return;
	}
	world.query_builder<Crabnado>().build().each([](const flecs::entity crabnado, Crabnado&) { crabnado.disable(); });
}

std::string ResolveSourcePath(const std::string_view assetDirectory, const std::string& path) {
	return std::filesystem::path{path}.is_absolute() ? path : std::string{assetDirectory} + "/" + path;
}

void LoadCampaignData(const flecs::world& world, const std::string_view assetDirectory) {
	const auto& source = world.get<CampaignSource>();
	const auto resolve = [assetDirectory](const std::string& path) { return ResolveSourcePath(assetDirectory, path); };
	LoadBosses(world, resolve(source.bosses_path));

	std::vector<std::string> enemyIds;
	for (const EnemyType& type : world.get<EnemyRegistry>().types) {
		enemyIds.push_back(type.id);
	}
	std::vector<std::string> bossIds;
	for (const BossDef& boss : world.get<BossRegistry>().bosses) {
		bossIds.push_back(boss.id);
	}

	const std::string path = resolve(source.campaign_path);
	const auto scriptExists = [&world](const std::string_view id) { return std::filesystem::exists(ScriptPath(world, id)); };
	CampaignParseResult result;
	if (const auto doc = core::LoadJsonFile(path, kLogTag)) {
		result = ParseCampaign(*doc, enemyIds, bossIds, SpawnerIds(), scriptExists);
	}
	for (const std::string& error : result.errors) {
		spdlog::error("[{}] '{}': {}", kLogTag, path, error);
	}
	spdlog::info("[{}] Loaded {} acts from '{}'", kLogTag, result.campaign.acts.size(), path);
	world.set<Campaign>(std::move(result.campaign));
}

float SpawnRate(const RateCurve& rate, const float stageClock) {
	const float value = rate.base + rate.growth * stageClock;
	return rate.max ? std::min(*rate.max, value) : value;
}

float BossSpawnRate(const Act& act) {
	if (!act.boss || act.stages.empty()) {
		return 0.0F;
	}
	const Stage& last = act.stages.back();
	return SpawnRate(last.rate, last.gate.type == GateType::Time ? last.gate.time : 0.0F) * act.boss->rate_mult;
}

float EnemyHpScale(const Act& act, const Stage* stage, const float actClock) {
	const HpScale& scale = stage != nullptr && stage->hp_scale ? *stage->hp_scale : act.hp_scale;
	const float growth = scale.growth_seconds > 0.0F ? 1.0F + actClock / scale.growth_seconds : 1.0F;
	return scale.mult * growth;
}

const Act* CurrentAct(const flecs::world& world) {
	const auto& acts = world.get<Campaign>().acts;
	const int index = world.get<RunState>().act;
	return index >= 0 && index < static_cast<int>(acts.size()) ? &acts[static_cast<size_t>(index)] : nullptr;
}

const Stage* CurrentStage(const flecs::world& world) {
	const Act* act = CurrentAct(world);
	const int index = world.get<RunState>().stage;
	return act != nullptr && index >= 0 && index < static_cast<int>(act->stages.size()) ? &act->stages[static_cast<size_t>(index)]
																						: nullptr;
}

bool InBossStage(const flecs::world& world) {
	const Act* act = CurrentAct(world);
	return act != nullptr && world.get<RunState>().stage >= static_cast<int>(act->stages.size());
}

std::optional<SpawnPlan> CurrentSpawnPlan(const flecs::world& world) {
	const Act* act = CurrentAct(world);
	if (act == nullptr) {
		return std::nullopt;
	}
	if (const Stage* stage = CurrentStage(world)) {
		return SpawnPlan{&stage->spawns, SpawnRate(stage->rate, world.get<RunState>().stage_clock)};
	}
	if (act->boss) {
		return SpawnPlan{&act->boss->spawns, BossSpawnRate(*act)};
	}
	return std::nullopt;
}

float CurrentEnemyHpScale(const flecs::world& world) {
	const Act* act = CurrentAct(world);
	return act == nullptr ? 1.0F : EnemyHpScale(*act, CurrentStage(world), world.get<RunState>().act_clock);
}

namespace {

// Pays a `both` gate's bonus.
void PayBonus(const flecs::world& world, RunState& run, const GateBonus& bonus) {
	switch (bonus.type) {
	case BonusType::Shells:
		// The same path a pickup uses: MoltCheck spends them next tick, so a big enough bonus molts.
		run.shells += static_cast<int>(std::lround(bonus.amount.value_or(0.5F * static_cast<float>(run.shells_needed))));
		break;
	case BonusType::Reroll: run.rerolls += static_cast<int>(std::lround(bonus.amount.value_or(1.0F))); break;
	case BonusType::Heal:
		if (const flecs::entity player = FindPlayer(world)) {
			auto& stats = player.get_mut<PlayerStats>();
			stats.hp = std::min(stats.max_hp, stats.hp + bonus.amount.value_or(0.3F * stats.max_hp));
		}
		break;
	}
}

// Enters the Stage RunState names: loads its script, then a normal Stage shows its banner, the Boss Stage starts the
// storm-up (or, with no boss, clears the Act), and a normal Stage also ends a boss phase a jump left behind. Fires
// OnStageStart.
void EnterStage(const flecs::world& world, RunState& run, const Act& act) {
	run.stage_kills_start = run.kills;
	run.storm_up = 0.0F;
	const bool normal = run.stage < static_cast<int>(act.stages.size());
	SetCrabnadoPresent(world, act.crabnado || !normal);
	LoadStageScript(world, normal ? std::string_view{act.stages[static_cast<size_t>(run.stage)].script} : std::string_view{});
	if (normal) {
		if (run.phase == RunPhase::Boss) {
			run.phase = RunPhase::Playing;
			run.slams = 0;
		}
		run.stage_banner = constants::kStageBannerTime;
		PlaySfx(world, Sfx::StageSting);
		spdlog::info("[Run] Act {} Stage {} '{}' at {:.1f}s", run.act, run.stage, act.stages[static_cast<size_t>(run.stage)].name, run.clock);
		FireCampaignEvent(world, "OnStageStart");
		return;
	}
	run.stage_banner = 0.0F;
	if (!act.boss) {
		spdlog::warn("[Run] Act {} has no boss: cleared at {:.1f}s", run.act, run.clock);
		run.phase = RunPhase::Boss;
		ClearAct(world);
		return;
	}
	if (run.phase == RunPhase::Playing) {
		run.storm_up = constants::kStormUpTime;
		PlaySfx(world, Sfx::StormWarning);
		spdlog::info("[Run] The storm rises at {:.1f}s", run.clock);
	}
	FireCampaignEvent(world, "OnStageStart");
}

// The boss comes ashore: the Boss phase starts with its banner, and OnBossStart fires.
void BeginBoss(const flecs::world& world, RunState& run, const Act& act) {
	run.phase = RunPhase::Boss;
	run.banner_timer = constants::kBossBannerTime;
	spdlog::info("[Run] The crabnado comes ashore at {:.1f}s", run.clock);
	FireCampaignEvent(world, "OnBossStart", {act.boss ? act.boss->id : std::string{}});
}

// Whether a Stage's gate has opened, and whether it opened with a `both` gate's quota met (in time, or at the same
// tick as the clock).
// `reason` is what OnStageEnd tells a script: "time" or "quota".
struct GateResult {
	bool open{false};
	bool challengeMet{false};
	std::string_view reason{"time"};
};

// A `script` gate opens only through EndStage() (EndStageEarly), unless its script died: then it is a time gate of
// kScriptGateFallbackTime, so a crashed script can't hold the run.
GateResult EvaluateGate(const Gate& gate, const RunState& run, const bool scriptAlive) {
	const bool timeUp = run.stage_clock >= gate.time;
	const bool quotaMet = run.kills - run.stage_kills_start >= gate.kills;
	switch (gate.type) {
	case GateType::Time: return {.open = timeUp};
	case GateType::Quota: return {.open = quotaMet, .reason = "quota"};
	case GateType::Both: return {.open = timeUp || quotaMet, .challengeMet = quotaMet, .reason = quotaMet ? "quota" : "time"};
	case GateType::Script: return {.open = !scriptAlive && run.stage_clock >= constants::kScriptGateFallbackTime};
	}
	return {};
}

// Ends the current Stage for `reason` and enters the next: pays a met `both` challenge's bonus, fires OnStageEnd to the
// scripts, then moves on.
void AdvanceStage(const flecs::world& world, RunState& run, const Act& act, const std::string_view reason, const bool challengeMet) {
	if (challengeMet) {
		const Stage& stage = act.stages[static_cast<size_t>(run.stage)];
		PayBonus(world, run, stage.gate.bonus);
		run.challenge_cleared = constants::kChallengeClearedTime;
		PlaySfx(world, Sfx::ChallengeCleared);
		spdlog::info("[Run] Challenge cleared in Stage {} at {:.1f}s", run.stage, run.clock);
	}
	FireCampaignEvent(world, "OnStageEnd", {std::string{reason}});
	++run.stage;
	world.get_mut<DirectorApplied>().stage = run.stage;
	run.stage_clock = 0.0F;
	EnterStage(world, run, act);
}

// One director tick: enter the Stage RunState names if it differs from the last applied pair; in the Boss Stage count
// the storm-up down and land the boss; else end the current Stage when its gate opens.
void StepDirector(const flecs::world& world, RunState& run, const float dt) {
	if (!IsRunning(run.phase)) {
		return;
	}
	const Act* act = CurrentAct(world);
	if (act == nullptr) {
		return;
	}
	auto& applied = world.get_mut<DirectorApplied>();
	if (run.act != applied.act || run.stage != applied.stage) {
		const bool actChanged = run.act != applied.act;
		if (actChanged) {
			run.act_clock = 0.0F;
			LoadActScript(world, act->script);
		}
		applied = {.act = run.act, .stage = run.stage};
		run.stage_clock = 0.0F;
		if (actChanged) {
			FireCampaignEvent(world, "OnActStart");
		}
		EnterStage(world, run, *act);
		return;
	}
	const Stage* stage = CurrentStage(world);
	if (stage == nullptr) {
		if (run.storm_up > 0.0F && run.phase == RunPhase::Playing) {
			run.storm_up = std::max(0.0F, run.storm_up - dt);
			if (run.storm_up <= 0.0F) {
				BeginBoss(world, run, *act);
			}
		}
		return;
	}
	const GateResult gate = EvaluateGate(stage->gate, run, StageScriptAlive(world));
	if (gate.open) {
		AdvanceStage(world, run, *act, gate.reason, gate.challengeMet);
	}
}

} // namespace

std::string_view EndStageEarly(const flecs::world& world) {
	auto& run = world.get_mut<RunState>();
	if (IsMolting(run.phase, run.step)) {
		return "in a molt";
	}
	if (run.phase == RunPhase::Transition) {
		return "in a transition";
	}
	const Act* act = CurrentAct(world);
	const Stage* stage = CurrentStage(world);
	if (act == nullptr || stage == nullptr) {
		return "in a Boss Stage";
	}
	if (run.phase != RunPhase::Playing) {
		return "the run isn't playing";
	}
	const bool quotaMet = stage->gate.type == GateType::Both && run.kills - run.stage_kills_start >= stage->gate.kills;
	AdvanceStage(world, run, *act, "script", quotaMet);
	return {};
}

std::string_view CallBossNow(const flecs::world& world, const std::string_view bossId) {
	auto& run = world.get_mut<RunState>();
	const Act* act = CurrentAct(world);
	if (act == nullptr || !InBossStage(world) || !act->boss) {
		return "not in the Act's Boss Stage";
	}
	if (act->boss->id != bossId) {
		return "not this Act's boss";
	}
	if (run.phase != RunPhase::Playing) {
		return run.phase == RunPhase::Boss ? "the boss is already up" : "the run isn't playing";
	}
	run.storm_up = 0.0F;
	BeginBoss(world, run, *act);
	return {};
}

std::string FormatClock(const float seconds) {
	const int whole = std::max(0, static_cast<int>(std::ceil(seconds)));
	return std::format("{}:{:02}", whole / 60, whole % 60);
}

std::string ChallengeText(const Gate& gate) {
	return gate.type == GateType::Both ? std::format("Defeat {} in {}!", gate.kills, FormatClock(gate.time)) : std::string{};
}

void RegisterCampaign(const flecs::world& world) {
	world.component<Campaign>();
	world.component<DirectorApplied>();
	world.component<CampaignSource>()
		.member<std::string>("campaign_path")
		.member<std::string>("bosses_path")
		.member<std::string>("scripts_path");
	world.set<CampaignSource>({});
	world.component<BossRegistry>();
	world.set<Campaign>({});
	world.set<DirectorApplied>({});
	world.set<BossRegistry>({});

	world.system<RunState>("CampaignDirector")
		.term_at(0)
		.src<RunState>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, RunState& run) {
			StepDirector(it.world(), run, it.delta_time());
			StepCampaignScripts(it.world(), it.delta_time());
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

} // namespace crabnado
