#pragma once

#include <string>

#include <flecs.h>

#include "football_components.hpp"

namespace game::football {

// The segment currently being played (from GameMode). Falls back to a default normal segment.
[[nodiscard]] SegmentDef CurrentSegment(const flecs::world& world);

// Is the current segment the boss drive (touchdown / field goal)?
[[nodiscard]] bool IsBoss(const flecs::world& world);

// Weighted-mean yardage of a play — its base "expected yards".
[[nodiscard]] float PlayAverageYards(const PlayDef& def);

// A play's odds as they will actually resolve this down, after the currently-applied HandModifiers.
struct PlayStats {
	float avg_yards{0.0F};
	float turnover{0.0F}; // [0,1]
	float flag{0.0F};     // [0,1]
};
[[nodiscard]] PlayStats EffectivePlayStats(const flecs::world& world, const PlayDef& def);

// Discard the current hand and draw a new one (up to kHandSize), reshuffling the discard back in when
// the draw pile empties. Resets HandModifiers for the new down. The draw pile persists across the
// whole game (built once in InitRun), so drawing genuinely cycles the deck.
void DrawHand(const flecs::world& world);

// Add a bought card to the deck and shuffle it into the draw pile so it can come up this game.
void AddToDeck(const flecs::world& world, const Card& card);

// Play the modifier card at `hand_index` (must be a Modifier): fold its effect into HandModifiers and
// move it to the discard. Clears any staged play selection (odds changed — re-pick).
void ApplyModifier(const flecs::world& world, int hand_index);

// Resolve `play_id` against the odds engine, applying HandModifiers. Deterministic via RngState.
[[nodiscard]] LastOutcome ResolvePlay(const flecs::world& world, const std::string& play_id);

// Populate the ShopState with kShopOffers random cards (plays + modifiers) drawn from the registries.
void GenerateShopOffers(const flecs::world& world);

} // namespace game::football
