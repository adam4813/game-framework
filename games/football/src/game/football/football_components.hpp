#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/core/registry.hpp"

// Football-specific ECS data. Game-only, built on genre-agnostic engine pieces: engine::core::Registry<T>
// for id-keyed lookup and engine::ecs::RngState for deterministic sampling/shuffling.
namespace game::football {

// One weighted yardage outcome of a play. Sampling a play picks a bucket in proportion to weight.
struct YardBucket {
	int yards{0};
	float weight{1.0F};
};

// Immutable definition of a play, parsed from assets/data/football/plays/<id>.json. Plain data.
struct PlayDef {
	std::string name;              // display name ("Slant")
	std::string category{"run"};   // "run" | "pass" | "trick" — drives odds flavour + the pick animation
	std::vector<YardBucket> yards; // weighted yardage table sampled on resolve
	float turnover_chance{0.0F};   // probability [0,1] the play is a turnover (0 yards, drive ends)
	float flag_chance{0.0F};       // probability [0,1] of a penalty flag (negated by "pay off ref")
	int cost{8};                   // shop price in coins
};

// Immutable definition of a modifier card: a utility ("cheat") or a buff/debuff that is *played onto
// the hand* to change the remaining plays' odds this down. Parsed from
// assets/data/football/modifiers/<id>.json. The `effect` key selects how it changes the hand.
struct ModifierDef {
	std::string name;
	std::string category;    // "utility" | "buff" | "debuff" (display flavour)
	std::string effect;      // "no_flag" | "no_turnover" | "add_yards" | "reduce_turnover" | "reduce_flag" | "risky"
	std::string description; // short flavour/help line shown on the card
	int amount{0};           // effect magnitude (yards, or percentage points for the *_chance effects)
	int cost{8};             // shop price in coins
};

using PlayRegistry = engine::core::Registry<PlayDef>;
using ModifierRegistry = engine::core::Registry<ModifierDef>;

// Kind of a card held in the deck/hand: a callable play, or a modifier (utility/buff/debuff) that is
// played onto the hand.
enum class CardKind : std::uint8_t { Play, Modifier };

// One physical card. `id` indexes PlayRegistry (Play) or ModifierRegistry (Modifier).
struct Card {
	CardKind kind{CardKind::Play};
	std::string id;
};

// The player's whole collection. Grows when cards are bought in the shop.
struct Deck {
	std::vector<Card> cards;
};

// Draw pile + discard implement a standard deckbuilder cycle. The draw pile is (re)built by shuffling
// the deck at each segment start; drawing a hand pulls from it, reshuffling the discard back in when
// it empties.
struct DrawPile {
	std::vector<Card> cards;
};
struct DiscardPile {
	std::vector<Card> cards;
};

// The hand currently in play (up to kHandSize cards), redrawn each down.
struct Hand {
	std::vector<Card> cards;
};

// Modifiers accumulated by playing utility/buff/debuff cards this down, applied when the play is
// hiked. Reset when a fresh hand is drawn.
struct HandModifiers {
	int bonus_yards{0};
	float turnover_delta{0.0F}; // subtracted from the play's turnover chance
	float flag_delta{0.0F};     // subtracted from the play's flag chance
	float extra_turnover{0.0F}; // added by risky/debuff cards
	bool no_flag{false};
	bool no_turnover{false};
};

// Result of resolving the most recently called play, shown on the resolve screen + pick animation.
struct LastOutcome {
	std::string play_id;
	std::string play_name;
	std::string category;
	int yards{0};
	bool turnover{false};
	bool flagged{false};
};

// One purchasable card in the shop.
struct ShopOffer {
	Card card;
	int cost{0};
	bool purchased{false};
};
struct ShopState {
	std::vector<ShopOffer> offers;
};

// A segment of the drive. `boss` segments score a touchdown/field goal instead of a first down.
// `target` is the yardage marker; `fg_yards` is the field-goal threshold (boss only).
struct SegmentDef {
	bool boss{false};
	int target{10};
	int fg_yards{0};
};

// The run "mode": the ordered list of segments. Drives segment count + targets everywhere.
struct GameMode {
	std::vector<SegmentDef> segments;
};

// Whole-game run state singleton. Aggregate (no constructors) so `set<FootballState>({})` works.
struct FootballState {
	int segment_index{0};
	int down{1};
	int yards{0};
	int target{10};
	int total_points{0};
	int coins{0};
	int segments_converted{0};
	bool last_converted{false};
	bool boss_touchdown{false};
	bool boss_field_goal{false};
	std::string selected_play;   // play id staged by selecting a play card (drives the Hike + resolve)
	int selected_hand_index{-1}; // which hand card is the staged play (-1 = none selected yet)
};

// Best final score across runs (persisted). Aggregate singleton so `set<HighScore>({})` works.
struct HighScore {
	int best_points{0};
};

// Identity tag for the football scene (switch-by-name over the Flecs Remote API, stable across runs).
struct FootballSceneTag {};

// Phase tags. Exactly one lives on the football "flow" entity at a time; football systems and
// observers key off them (a flecs-idiomatic state machine — no string state read imperatively).
namespace phase {
struct PlaySelect {};
struct Resolve {};
struct SegmentEnd {};
struct Shop {};
struct Results {};
} // namespace phase

// Tag added to the flow entity to request a rebuild of the current phase's UI next frame (used for
// intra-phase changes like a shop purchase, playing a modifier, or opening the deck overlay).
struct RebuildUITag {};

} // namespace game::football
