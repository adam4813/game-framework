#include "campaign.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "boss_registry.hpp"
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
constexpr std::array<std::string_view, 1> kSpawnerIds{"crabnado"};

// What the parsers share. A fatal problem in a Stage or Act throws std::runtime_error, caught where that Stage or Act
// is dropped; a droppable one (a bad spawn entry) goes straight into `errors`.
struct Context {
	std::span<const std::string> enemyIds;
	std::span<const std::string> bossIds;
	std::span<const std::string_view> spawnerIds;
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
	gate.script = Text(j, "script");
	return gate;
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
	act.arena = ParseArena(Object(j, "arena"));
	const json& sea = Object(j, "sea");
	act.sea = {.cross_swell_at = OptionalNumber(sea, "cross_swell_at"), .rogue_at = OptionalNumber(sea, "rogue_at")};
	act.hp_scale = ParseHpScale(Object(j, "hp_scale"));

	if (j.contains("stages")) {
		if (!j["stages"].is_array()) {
			throw std::runtime_error("'stages' must be an array");
		}
		int index = 0;
		for (const json& entry : j["stages"]) {
			const std::string stageWhere = std::format("{} stage {}", where, index++);
			try {
				act.stages.push_back(ParseStage(entry, stageWhere, ctx));
			} catch (const std::exception& ex) {
				ctx.errors.push_back(std::format("{}: {}, stage skipped", stageWhere, ex.what()));
			}
		}
	}
	if (j.contains("boss")) {
		const std::string bossWhere = std::format("{} boss", where);
		try {
			BossStage boss = ParseBossStage(Object(j, "boss"), bossWhere, ctx);
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
	const std::span<const std::string_view> spawnerIds
) {
	CampaignParseResult result;
	Context ctx{enemyIds, bossIds, spawnerIds, result.errors};
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

void LoadCampaignData(const flecs::world& world, const std::string_view assetDirectory) {
	const auto& source = world.get<CampaignSource>();
	const auto resolve = [assetDirectory](const std::string& path) {
		return std::filesystem::path{path}.is_absolute() ? path : std::string{assetDirectory} + path;
	};
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
	CampaignParseResult result;
	if (const auto doc = core::LoadJsonFile(path, kLogTag)) {
		result = ParseCampaign(*doc, enemyIds, bossIds, SpawnerIds());
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

// Enters the Stage RunState names: a normal Stage shows its banner, the Boss Stage starts the storm-up (or, with no
// boss, clears the Act, which for now wins the run), and a normal Stage also ends a boss phase a jump left behind.
void EnterStage(const flecs::world& world, RunState& run, const Act& act) {
	run.stage_kills_start = run.kills;
	run.storm_up = 0.0F;
	if (run.stage < static_cast<int>(act.stages.size())) {
		if (run.phase == RunPhase::Boss) {
			run.phase = RunPhase::Playing;
			run.slams = 0;
		}
		run.stage_banner = constants::kStageBannerTime;
		PlaySfx(world, Sfx::StageSting);
		spdlog::info("[Run] Act {} Stage {} '{}' at {:.1f}s", run.act, run.stage, act.stages[static_cast<size_t>(run.stage)].name, run.clock);
		return;
	}
	run.stage_banner = 0.0F;
	if (!act.boss) {
		spdlog::warn("[Run] Act {} has no boss: cleared at {:.1f}s", run.act, run.clock);
		run.phase = RunPhase::Boss;
		WinRun(world);
		return;
	}
	if (run.phase == RunPhase::Playing) {
		run.storm_up = constants::kStormUpTime;
		PlaySfx(world, Sfx::StormWarning);
		spdlog::info("[Run] The storm rises at {:.1f}s", run.clock);
	}
}

// Whether a Stage's gate has opened, and whether it opened with a `both` gate's quota met (in time, or at the same
// tick as the clock).
struct GateResult {
	bool open{false};
	bool challengeMet{false};
};

GateResult EvaluateGate(const Gate& gate, const RunState& run) {
	const bool timeUp = run.stage_clock >= gate.time;
	const bool quotaMet = run.kills - run.stage_kills_start >= gate.kills;
	switch (gate.type) {
	case GateType::Time: return {.open = timeUp};
	case GateType::Quota: return {.open = quotaMet};
	case GateType::Both: return {.open = timeUp || quotaMet, .challengeMet = quotaMet};
	case GateType::Script: break;
	}
	return {};
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
		if (run.act != applied.act) {
			run.act_clock = 0.0F;
		}
		applied = {.act = run.act, .stage = run.stage};
		run.stage_clock = 0.0F;
		EnterStage(world, run, *act);
		return;
	}
	const Stage* stage = CurrentStage(world);
	if (stage == nullptr) {
		if (run.storm_up > 0.0F && run.phase == RunPhase::Playing) {
			run.storm_up = std::max(0.0F, run.storm_up - dt);
			if (run.storm_up <= 0.0F) {
				run.phase = RunPhase::Boss;
				run.banner_timer = constants::kBossBannerTime;
				spdlog::info("[Run] The crabnado comes ashore at {:.1f}s", run.clock);
			}
		}
		return;
	}
	const GateResult gate = EvaluateGate(stage->gate, run);
	if (!gate.open) {
		return;
	}
	if (gate.challengeMet) {
		PayBonus(world, run, stage->gate.bonus);
		run.challenge_cleared = constants::kChallengeClearedTime;
		PlaySfx(world, Sfx::ChallengeCleared);
		spdlog::info("[Run] Challenge cleared in Stage {} at {:.1f}s", run.stage, run.clock);
	}
	++run.stage;
	applied.stage = run.stage;
	run.stage_clock = 0.0F;
	EnterStage(world, run, *act);
}

} // namespace

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
	world.component<CampaignSource>().member<std::string>("campaign_path").member<std::string>("bosses_path");
	world.set<CampaignSource>({});
	world.component<BossRegistry>();
	world.set<Campaign>({});
	world.set<DirectorApplied>({});
	world.set<BossRegistry>({});

	world.system<RunState>("CampaignDirector")
		.term_at(0)
		.src<RunState>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, RunState& run) { StepDirector(it.world(), run, it.delta_time()); })
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

} // namespace crabnado
