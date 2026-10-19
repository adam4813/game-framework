#include "score_systems.hpp"

#include <algorithm>
#include <cmath>

#include "score_components.hpp"
#include "ticket_components.hpp"
#include "ticket_rules.hpp"

namespace signal_room {

namespace {

template<typename Event>
void RaiseOnScore(const flecs::world& world, const Event& event) {
	world.event<Event>().template id<Score>().entity(world.entity<Score>()).ctx(event).emit();
}

float BandShare(const TicketRules& rules, const Ticket& ticket) {
	const auto* band = FindBand(rules, ticket);
	return band == nullptr ? 1.0F : band->share;
}

void ApplyDelta(const flecs::world& world, Score& score, const int delta, const ScoreReason reason) {
	const int applied = std::max(0, score.score + delta) - score.score;
	score.score += applied;
	RaiseOnScore(world, ScoreChanged{.delta = applied, .reason = reason});
}

void SetChain(const flecs::world& world, Score& score, const int step) {
	if (step == score.chainStep) {
		return;
	}
	score.chainStep = step;
	score.bestChainStep = std::max(score.bestChainStep, step);
	RaiseOnScore(world, ChainChanged{.step = step});
}

void OnCleared(const flecs::world& world, const Ticket& ticket) {
	const auto& rules = world.get<TicketRules>();
	auto& score = world.get_mut<Score>();
	const int lastStep = std::max(0, static_cast<int>(rules.handlerChain.size()) - 1);
	const float multiplier = rules.handlerChain.empty() ? 1.0F : rules.handlerChain[static_cast<size_t>(score.chainStep)];
	const int value = static_cast<int>(
		std::lround(static_cast<float>(rules.tiers.Get(ticket.tier)->base) * BandShare(rules, ticket) * multiplier)
	);
	++score.cleared;
	ApplyDelta(world, score, value, ScoreReason::Clear);
	SetChain(world, score, std::min(score.chainStep + 1, lastStep));
}

void OnLost(const flecs::world& world) {
	auto& score = world.get_mut<Score>();
	++score.lost;
	ApplyDelta(world, score, -world.get<TicketRules>().lostInterceptPenalty, ScoreReason::LostIntercept);
	SetChain(world, score, 0);
}

int StarsFor(const Score& score, const StarRules& rules) {
	int stars = 0;
	if (!rules.absolute.empty()) {
		for (const float threshold : rules.absolute) {
			stars += static_cast<float>(score.score) >= threshold ? 1 : 0;
		}
		return stars;
	}
	if (score.par <= 0) {
		return 0;
	}
	for (const float fraction : rules.fractions) {
		// Epsilon so 0.65 x par that lands on a whole score isn't lost to float rounding.
		stars += static_cast<double>(score.score) + 1e-4 >= static_cast<double>(fraction) * score.par ? 1 : 0;
	}
	return std::min(stars, 3);
}

} // namespace

void StartScore(const flecs::world& world, const Shift& shift) {
	world.set<Score>({});
	world.set<StarRules>(
		{.fractions = shift.starFractions.empty() ? world.get<TicketRules>().starFractions : shift.starFractions,
		 .absolute = shift.starAbsolute}
	);
}

void RegisterScoreSystems(const flecs::world& world, const flecs::entity roomRoot) {
	world.observer<const Ticket>("ScoreClear")
		.event<TicketCleared>()
		.each([](flecs::iter& it, size_t, const Ticket& ticket) { OnCleared(it.world(), ticket); })
		.child_of(roomRoot);
	world.observer<const Ticket>("ScoreLostIntercept")
		.event<LostIntercept>()
		.each([](flecs::iter& it, size_t, const Ticket&) { OnLost(it.world()); })
		.child_of(roomRoot);
	world.observer<const Ticket>("ScorePar")
		.event<TicketOnAir>()
		.each([](flecs::iter& it, size_t, const Ticket& ticket) {
			const auto world = it.world();
			world.get_mut<Score>().par += world.get<TicketRules>().tiers.Get(ticket.tier)->base;
		})
		.child_of(roomRoot);
	world.observer<ShiftWindow>("ScoreStars")
		.event<ShiftEnded>()
		.each([](flecs::iter& it, size_t, ShiftWindow&) {
			const auto world = it.world();
			auto& score = world.get_mut<Score>();
			score.stars = StarsFor(score, world.get<StarRules>());
		})
		.child_of(roomRoot);
}

} // namespace signal_room
