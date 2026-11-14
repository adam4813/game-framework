#pragma once

#include <array>
#include <string_view>

// Shared tuning constants for the football prototype. All balance values live here as constexpr so
// tuning never requires touching gameplay logic (data-driven design; magic numbers stay out of
// systems). Play definitions themselves live in JSON under games/football/assets/data/plays/.
namespace game::football::constants {

// ── Drive / segment rules ──
inline constexpr int kFirstDownYards = 10;  // yards needed to convert a normal first-down segment
inline constexpr int kDownsPerSegment = 4;  // plays available to reach the marker
inline constexpr int kNormalSegments = 3;   // first-down segments before the boss
inline constexpr int kBossSegmentIndex = 3; // segmentIndex value that means "the boss drive"

// ── Boss drive (touchdown / field goal) ──
inline constexpr int kBossTouchdownYards = 25; // reach this to score a touchdown
inline constexpr int kBossFieldGoalYards = 15; // fall short but reach this to kick a field goal

// ── Scoring ──
// Converting a normal segment scores the *bonus* yardage gained past the marker, capped at
// kOverageCap so a lucky big play can't run away with the score. Missing the marker scores nothing.
inline constexpr int kOverageCap = 5;       // cap on bonus yards counted past a marker (points + coins)
inline constexpr int kTouchdownPoints = 50; // flat reward for a boss touchdown (+ capped overshoot)
inline constexpr int kFieldGoalPoints = 20; // reward for a boss field goal

// A penalty flag negates the play and loses ground (unless a "pay off ref" utility is played).
inline constexpr int kFlagPenaltyYards = 10;

// ── Economy ──
inline constexpr int kStartingCoins = 0;
// Coins earned per segment equal the yards actually gained (distance covered), overage-capped so the
// economy scales with real progress rather than a lucky bomb. With ~8-15 coins a segment, cards are
// priced 7-14 so a shop visit is a real one-card-ish decision.
inline constexpr int kShopOffers = 3; // items offered for purchase between segments

// ── Hand / deck ──
inline constexpr int kHandSize = 3; // cards drawn from the deck each down (plus the 2 basic plays below)

// Always-available basic plays, shown every down on top of the drawn hand so the player can always
// move the ball. Their JSON lives in plays/ with cost 0; they are never in the deck or the shop.
inline constexpr std::string_view kBasicRun = "basic_run";
inline constexpr std::string_view kBasicPass = "basic_pass";

// Starting deck (the "playbook" the player can inspect). Ids reference games/football/assets/data/plays/.
inline constexpr std::array<std::string_view, 9> kStartingPlays{
	"run_dive", "run_dive", "off_tackle", "short_pass", "short_pass", "slant", "deep_pass", "screen", "qb_sneak",
};

// Modifier cards the player starts with in the deck (ids into games/football/assets/data/modifiers/).
inline constexpr std::array<std::string_view, 2> kStartingModifiers{
	"pay_off_ref",
	"playbook_boost",
};

// ── Persistence ──
inline constexpr std::string_view kHighScoreSlot = "football_high_score"; // platform save slot for HighScore

// ── Flow state ids (consumed by engine::flow::StateMachine) ──
namespace states {
inline constexpr std::string_view kPlaySelect = "play_select";
inline constexpr std::string_view kResolve = "resolve";
inline constexpr std::string_view kSegmentEnd = "segment_end";
inline constexpr std::string_view kShop = "shop";
inline constexpr std::string_view kResults = "results";
} // namespace states

} // namespace game::football::constants
