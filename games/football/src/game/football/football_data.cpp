#include "football_data.hpp"

#include <optional>
#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/assets/assets.hpp"
#include "engine/core/registry.hpp"
#include "engine/ecs/singletons.hpp"
#include "football_components.hpp"
#include "football_constants.hpp"

namespace game::football {

namespace {

std::optional<PlayDef> ParsePlay(const std::string_view id, const nlohmann::json& doc) {
	PlayDef def;
	def.name = doc.value("name", std::string{id});
	def.category = doc.value("category", std::string{"run"});
	def.turnover_chance = doc.value("turnoverChance", 0.0F);
	def.flag_chance = doc.value("flagChance", 0.0F);
	def.cost = doc.value("cost", 8);

	if (doc.contains("yards") && doc.at("yards").is_array()) {
		for (const auto& bucket : doc.at("yards")) {
			if (bucket.is_array() && bucket.size() >= 2) {
				def.yards.push_back({.yards = bucket[0].get<int>(), .weight = bucket[1].get<float>()});
			}
		}
	}
	if (def.yards.empty()) {
		spdlog::warn("[Football] Play '{}' has no yardage buckets; skipping", id);
		return std::nullopt;
	}
	return def;
}

std::optional<ModifierDef> ParseModifier(const std::string_view id, const nlohmann::json& doc) {
	ModifierDef def;
	def.name = doc.value("name", std::string{id});
	def.category = doc.value("category", std::string{"buff"});
	def.effect = doc.value("effect", std::string{});
	def.description = doc.value("description", std::string{});
	def.amount = doc.value("amount", 0);
	def.cost = doc.value("cost", 8);
	if (def.effect.empty()) {
		spdlog::warn("[Football] Modifier '{}' has no effect; skipping", id);
		return std::nullopt;
	}
	return def;
}

} // namespace

int LoadContent(const flecs::world& world) {
	const std::string plays_dir = engine::assets::ResolveAsset(world, "data/plays");
	PlayRegistry plays;
	const int loaded_plays = engine::core::LoadRegistryFromDirectory<PlayDef>(plays_dir, "Football", plays, ParsePlay);
	world.set<PlayRegistry>(std::move(plays));

	const std::string mods_dir = engine::assets::ResolveAsset(world, "data/modifiers");
	ModifierRegistry modifiers;
	const int loaded_mods =
		engine::core::LoadRegistryFromDirectory<ModifierDef>(mods_dir, "Football", modifiers, ParseModifier);
	world.set<ModifierRegistry>(std::move(modifiers));

	spdlog::info("[Football] Loaded {} plays, {} modifiers", loaded_plays, loaded_mods);
	return loaded_plays;
}

void InitRun(const flecs::world& world) {
	// Default mode: three first-down segments then a boss touchdown/field-goal drive.
	GameMode mode;
	for (int i = 0; i < constants::kNormalSegments; ++i) {
		mode.segments.push_back({.boss = false, .target = constants::kFirstDownYards});
	}
	mode.segments.push_back(
		{.boss = true, .target = constants::kBossTouchdownYards, .fg_yards = constants::kBossFieldGoalYards}
	);
	const int first_target = mode.segments.empty() ? constants::kFirstDownYards : mode.segments.front().target;
	world.set<GameMode>(std::move(mode));

	world.set<FootballState>({
		.segment_index = 0,
		.down = 1,
		.yards = 0,
		.target = first_target,
		.total_points = 0,
		.coins = constants::kStartingCoins,
		.segments_converted = 0,
		.selected_hand_index = -1,
	});

	Deck deck;
	deck.cards.reserve(constants::kStartingPlays.size() + constants::kStartingModifiers.size());
	for (const auto id : constants::kStartingPlays) {
		deck.cards.push_back({.kind = CardKind::Play, .id = std::string{id}});
	}
	for (const auto id : constants::kStartingModifiers) {
		deck.cards.push_back({.kind = CardKind::Modifier, .id = std::string{id}});
	}

	// The draw pile is built once for the whole game (a persistent deckbuilder cycle across segments).
	DrawPile draw;
	draw.cards = deck.cards;
	world.get_mut<engine::ecs::RngState>().Shuffle(draw.cards);

	world.set<Deck>(std::move(deck));
	world.set<DrawPile>(std::move(draw));
	world.set<DiscardPile>({});
	world.set<Hand>({});
	world.set<HandModifiers>({});
	world.set<LastOutcome>({});
	world.set<ShopState>({});
}

} // namespace game::football
