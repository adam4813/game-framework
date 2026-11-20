#include "boss_registry.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

#include <spdlog/spdlog.h>

#include "campaign.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "run.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr std::string_view kLogTag = "Bosses";
constexpr std::string_view kSlamBehavior = "slam";

// Reads every tunable of the slam behavior. A missing or non-numeric one is named in the returned error, with no
// default to fall back on.
std::string ParseSlam(const nlohmann::json& entry, const bool needsSlams, SlamTunables& out) {
	const std::array<std::pair<const char*, float*>, 13> floats{{
		{"orbit_radius", &out.orbit_radius},
		{"orbit_speed", &out.orbit_speed},
		{"gust_speed_mult", &out.gust_speed_mult},
		{"speed", &out.speed},
		{"knock_min_height", &out.knock_min_height},
		{"knock_push", &out.knock_push},
		{"radius", &out.radius},
		{"edge_margin", &out.edge_margin},
		{"contact_range", &out.contact_range},
		{"contact_dps", &out.contact_dps},
		{"slam_min_speed", &out.slam_min_speed},
		{"slam_cooldown", &out.slam_cooldown},
		{"slam_bounce", &out.slam_bounce},
	}};
	std::string missing;
	for (const auto& [key, field] : floats) {
		if (!entry.contains(key) || !entry[key].is_number()) {
			missing += std::format(" {}", key);
			continue;
		}
		*field = entry[key].get<float>();
	}
	if (needsSlams) {
		if (!entry.contains("slams_to_win") || !entry["slams_to_win"].is_number_integer()) {
			missing += " slams_to_win";
		} else {
			out.slams_to_win = entry["slams_to_win"].get<int>();
		}
	}
	const bool colored = entry.contains("storm_color") && entry["storm_color"].is_array() && entry["storm_color"].size() == 3 &&
		std::ranges::all_of(entry["storm_color"], [](const nlohmann::json& v) { return v.is_number_integer(); });
	if (!colored) {
		missing += " storm_color";
	} else {
		const auto channel = [&](const size_t i) { return static_cast<uint8_t>(std::clamp(entry["storm_color"][i].get<int>(), 0, 255)); };
		out.storm_color = {.r = channel(0), .g = channel(1), .b = channel(2), .a = 255};
	}
	return missing.empty() ? std::string{} : std::format("missing or non-numeric tunables:{}", missing);
}

// Reads the extra tunables of the enraged behavior, naming any that is missing.
std::string ParseEnraged(const nlohmann::json& entry, EnragedTunables& out) {
	const std::array<std::pair<const char*, float*>, 13> floats{{
		{"hp", &out.hp},
		{"slam_damage", &out.slam_damage},
		{"daze_time", &out.daze_time},
		{"chase_min", &out.chase_min},
		{"chase_max", &out.chase_max},
		{"recover", &out.recover},
		{"volley_radius", &out.volley_radius},
		{"volley_flight", &out.volley_flight},
		{"volley_impact", &out.volley_impact},
		{"suction_radius", &out.suction_radius},
		{"suction_warn", &out.suction_warn},
		{"suction_speed", &out.suction_speed},
		{"suction_time", &out.suction_time},
	}};
	std::string missing;
	for (const auto& [key, field] : floats) {
		if (!entry.contains(key) || !entry[key].is_number()) {
			missing += std::format(" {}", key);
			continue;
		}
		*field = entry[key].get<float>();
	}
	if (!entry.contains("volley_ring") || !entry["volley_ring"].is_number_integer()) {
		missing += " volley_ring";
	} else {
		out.volley_ring = entry["volley_ring"].get<int>();
	}
	return missing.empty() ? std::string{} : std::format("missing or non-numeric tunables:{}", missing);
}

} // namespace

BossParseResult ParseBosses(const nlohmann::json& doc) {
	BossParseResult result;
	if (!doc.is_object()) {
		result.errors.emplace_back("bosses.json must be an object of bosses keyed by id");
		return result;
	}
	for (const auto& [id, entry] : doc.items()) {
		if (!entry.is_object()) {
			result.errors.push_back(std::format("boss '{}' must be an object", id));
			continue;
		}
		const bool named = entry.contains("behavior") && entry["behavior"].is_string();
		BossDef def{.id = id, .behavior = named ? entry["behavior"].get<std::string>() : std::string{}, .tunables = entry, .slam = {}, .enraged = {}};
		const bool enraged = def.behavior == kEnragedBehavior;
		if (def.behavior != kSlamBehavior && !enraged) {
			result.errors.push_back(std::format("boss '{}': unknown behavior '{}'", id, def.behavior));
			continue;
		}
		std::string error = ParseSlam(entry, !enraged, def.slam);
		if (error.empty() && enraged) {
			error = ParseEnraged(entry, def.enraged);
		}
		if (!error.empty()) {
			result.errors.push_back(std::format("boss '{}': {}", id, error));
			continue;
		}
		result.registry.bosses.push_back(std::move(def));
	}
	return result;
}

void LoadBosses(const flecs::world& world, const std::string_view path) {
	BossParseResult result;
	if (const auto doc = core::LoadJsonFile(path, kLogTag)) {
		result = ParseBosses(*doc);
	}
	for (const std::string& error : result.errors) {
		spdlog::error("[{}] '{}': {}", kLogTag, path, error);
	}
	spdlog::info("[{}] Loaded {} bosses from '{}'", kLogTag, result.registry.bosses.size(), path);
	world.set<BossRegistry>(std::move(result.registry));
}

const BossDef* CurrentBoss(const flecs::world& world) {
	const Act* act = CurrentAct(world);
	if (act == nullptr || !act->boss) {
		return nullptr;
	}
	const auto& bosses = world.get<BossRegistry>().bosses;
	const auto found = std::ranges::find(bosses, act->boss->id, &BossDef::id);
	return found == bosses.end() ? nullptr : &*found;
}

float StormLevel(const flecs::world& world) {
	if (CurrentBoss(world) == nullptr || !InBossStage(world)) {
		return 0.0F;
	}
	const auto& run = world.get<RunState>();
	return run.phase == RunPhase::Boss ? 1.0F : std::clamp(1.0F - run.storm_up / constants::kStormUpTime, 0.0F, 1.0F);
}

} // namespace crabnado
