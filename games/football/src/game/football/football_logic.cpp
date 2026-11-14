#include "football_logic.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "engine/ecs/singletons.hpp"
#include "football_constants.hpp"

namespace game::football {

namespace {

constexpr float kRiskyTurnover = 0.15F; // extra turnover added by a "risky" debuff card

// Fold a modifier's effect into the accumulating HandModifiers.
void FoldModifier(const ModifierDef& def, HandModifiers& mods) {
	const float pct = static_cast<float>(def.amount) / 100.0F;
	if (def.effect == "no_flag") {
		mods.no_flag = true;
	}
	else if (def.effect == "no_turnover") {
		mods.no_turnover = true;
	}
	else if (def.effect == "add_yards") {
		mods.bonus_yards += def.amount;
	}
	else if (def.effect == "reduce_turnover") {
		mods.turnover_delta += pct;
	}
	else if (def.effect == "reduce_flag") {
		mods.flag_delta += pct;
	}
	else if (def.effect == "risky") {
		mods.bonus_yards += def.amount;
		mods.extra_turnover += kRiskyTurnover;
	}
}

} // namespace

SegmentDef CurrentSegment(const flecs::world& world) {
	const auto& state = world.get<FootballState>();
	const auto& mode = world.get<GameMode>();
	if (state.segment_index >= 0 && state.segment_index < static_cast<int>(mode.segments.size())) {
		return mode.segments[static_cast<std::size_t>(state.segment_index)];
	}
	return {.boss = false, .target = constants::kFirstDownYards};
}

bool IsBoss(const flecs::world& world) { return CurrentSegment(world).boss; }

float PlayAverageYards(const PlayDef& def) {
	float total_weight = 0.0F;
	float weighted = 0.0F;
	for (const auto& bucket : def.yards) {
		if (bucket.weight > 0.0F) {
			total_weight += bucket.weight;
			weighted += bucket.weight * static_cast<float>(bucket.yards);
		}
	}
	return total_weight > 0.0F ? weighted / total_weight : 0.0F;
}

PlayStats EffectivePlayStats(const flecs::world& world, const PlayDef& def) {
	const auto& mods = world.get<HandModifiers>();
	PlayStats stats;
	stats.avg_yards = PlayAverageYards(def) + static_cast<float>(mods.bonus_yards);
	stats.turnover =
		mods.no_turnover ? 0.0F : std::clamp(def.turnover_chance - mods.turnover_delta + mods.extra_turnover, 0.0F, 1.0F);
	stats.flag = mods.no_flag ? 0.0F : std::max(0.0F, def.flag_chance - mods.flag_delta);
	return stats;
}

void DrawHand(const flecs::world& world) {
	auto& hand = world.get_mut<Hand>();
	auto& draw = world.get_mut<DrawPile>();
	auto& discard = world.get_mut<DiscardPile>();
	auto& rng = world.get_mut<engine::ecs::RngState>();

	// Discard the previous hand.
	for (const auto& card : hand.cards) {
		discard.cards.push_back(card);
	}
	hand.cards.clear();

	for (int i = 0; i < constants::kHandSize; ++i) {
		if (draw.cards.empty()) {
			if (discard.cards.empty()) {
				break;
			}
			for (const auto& card : discard.cards) {
				draw.cards.push_back(card);
			}
			discard.cards.clear();
			rng.Shuffle(draw.cards);
		}
		if (draw.cards.empty()) {
			break;
		}
		hand.cards.push_back(draw.cards.back());
		draw.cards.pop_back();
	}

	world.get_mut<HandModifiers>() = {};
	auto& st = world.get_mut<FootballState>();
	st.selected_hand_index = -1;
	st.selected_play.clear();
}

void ApplyModifier(const flecs::world& world, const int hand_index) {
	auto& hand = world.get_mut<Hand>();
	if (hand_index < 0 || hand_index >= static_cast<int>(hand.cards.size())) {
		return;
	}
	const Card card = hand.cards[static_cast<std::size_t>(hand_index)];
	if (card.kind != CardKind::Modifier) {
		return;
	}
	if (const ModifierDef* def = world.get<ModifierRegistry>().Get(card.id)) {
		FoldModifier(*def, world.get_mut<HandModifiers>());
	}
	world.get_mut<DiscardPile>().cards.push_back(card);
	hand.cards.erase(hand.cards.begin() + hand_index);

	// The hand's odds changed and indices shifted — force a fresh play pick.
	auto& st = world.get_mut<FootballState>();
	st.selected_hand_index = -1;
	st.selected_play.clear();
}

LastOutcome ResolvePlay(const flecs::world& world, const std::string& play_id) {
	const auto& plays = world.get<PlayRegistry>();
	const auto& mods = world.get<HandModifiers>();
	auto& rng = world.get_mut<engine::ecs::RngState>();

	LastOutcome outcome;
	outcome.play_id = play_id;

	const PlayDef* def = plays.Get(play_id);
	if (!def) {
		outcome.play_name = play_id;
		return outcome;
	}
	outcome.play_name = def->name;
	outcome.category = def->category;

	const float turnover_chance =
		mods.no_turnover ? 0.0F : std::clamp(def->turnover_chance - mods.turnover_delta + mods.extra_turnover, 0.0F, 1.0F);
	const float flag_chance = mods.no_flag ? 0.0F : std::max(0.0F, def->flag_chance - mods.flag_delta);

	if (rng.Chance(turnover_chance)) {
		outcome.turnover = true;
		outcome.yards = 0;
		return outcome;
	}
	if (rng.Chance(flag_chance)) {
		outcome.flagged = true;
		outcome.yards = -constants::kFlagPenaltyYards;
		return outcome;
	}

	std::vector<float> weights;
	weights.reserve(def->yards.size());
	for (const auto& bucket : def->yards) {
		weights.push_back(bucket.weight);
	}
	const int index = rng.WeightedIndex(weights);
	outcome.yards = (index >= 0 ? def->yards[static_cast<std::size_t>(index)].yards : 0) + mods.bonus_yards;
	return outcome;
}

void GenerateShopOffers(const flecs::world& world) {
	const auto& plays = world.get<PlayRegistry>();
	const auto& modifiers = world.get<ModifierRegistry>();
	auto& shop = world.get_mut<ShopState>();
	auto& rng = world.get_mut<engine::ecs::RngState>();

	std::vector<ShopOffer> pool;
	for (const auto& id : plays.Ids()) {
		const PlayDef* def = plays.Get(id);
		if (def && def->cost <= 0) {
			continue; // basic (free/always-available) plays are never sold
		}
		pool.push_back({.card = {.kind = CardKind::Play, .id = id}, .cost = def ? def->cost : 8, .purchased = false});
	}
	for (const auto& id : modifiers.Ids()) {
		const ModifierDef* def = modifiers.Get(id);
		pool.push_back(
			{.card = {.kind = CardKind::Modifier, .id = id}, .cost = def ? def->cost : 8, .purchased = false}
		);
	}
	rng.Shuffle(pool);

	shop.offers.clear();
	const auto count = std::min<std::size_t>(constants::kShopOffers, pool.size());
	shop.offers.assign(pool.begin(), pool.begin() + static_cast<std::ptrdiff_t>(count));
}

void AddToDeck(const flecs::world& world, const Card& card) {
	world.get_mut<Deck>().cards.push_back(card);
	auto& draw = world.get_mut<DrawPile>();
	draw.cards.push_back(card);
	world.get_mut<engine::ecs::RngState>().Shuffle(draw.cards);
}

} // namespace game::football
