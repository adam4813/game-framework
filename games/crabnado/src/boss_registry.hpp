#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include <flecs.h>

// The boss registry: `bosses.json` keyed by boss id, each entry naming a `behavior` and that behavior's tunables.
// An Act's Boss Stage refers to an entry by id. Loaded on every arena scene Load, so an edit shows on the next Play.
namespace crabnado {

// The tunables of the `slam` behavior (the crabnado). Outside the Boss Stage it circles the barrier at `orbit_radius`
// units from the centre and `orbit_speed` units per second, faster by `gust_speed_mult` times the wind's gust. In the
// Boss Stage it chases the player ashore, and a crest that knocks it into a
// rock fast enough is a slam. A wave higher than `knock_min_height` carries it at `knock_push` of the wave push, else
// its knock decays. It is a circle of `radius` against rocks, stays `edge_margin` inside the arena edge, and within
// `contact_range` of the player it drains `contact_dps` HP per second. Knocked into a rock faster than `slam_min_speed`
// it takes a slam (`slam_cooldown` seconds apart; knock * `slam_bounce`); `slams_to_win` beat it.
// Every field is required: a missing one is a validation error and the entry is skipped.
struct SlamTunables {
	float orbit_radius{0.0F};
	float orbit_speed{0.0F};
	float gust_speed_mult{0.0F};
	float speed{0.0F};
	float knock_min_height{0.0F};
	float knock_push{0.0F};
	float radius{0.0F};
	float edge_margin{0.0F};
	float contact_range{0.0F};
	float contact_dps{0.0F};
	float slam_min_speed{0.0F};
	float slam_cooldown{0.0F};
	float slam_bounce{0.0F};
	int slams_to_win{0};
};

// One boss: `tunables` is the entry as authored, `slam` its parsed form when `behavior` is "slam".
struct BossDef {
	std::string id;
	std::string behavior;
	nlohmann::json tunables;
	SlamTunables slam;
};

// Singleton: every valid boss, in id order.
struct BossRegistry {
	std::vector<BossDef> bosses;
};

struct BossParseResult {
	BossRegistry registry;
	// One line per skipped entry or bad document, ready to log.
	std::vector<std::string> errors;
};

// Parses a bosses.json document. A bad entry (not an object, an unknown behavior, a missing tunable) is reported in
// `errors` and skipped. Pure, so tests can call it without the engine.
[[nodiscard]] BossParseResult ParseBosses(const nlohmann::json& doc);

// Replaces the BossRegistry singleton with the bosses in the JSON file at `path`, logging each error.
void LoadBosses(const flecs::world& world, std::string_view path);

// The slam boss of the current Act's Boss Stage, or nullptr when the Act has none (or it isn't a slam boss).
[[nodiscard]] const BossDef* CurrentBoss(const flecs::world& world);

// How far the storm has built, 0 to 1: it ramps over the Boss Stage's storm-up (RunState::storm_up counting down from
// kStormUpTime) and holds at 1 once the boss lands. 0 before the Boss Stage, and with no boss.
[[nodiscard]] float StormLevel(const flecs::world& world);

} // namespace crabnado
