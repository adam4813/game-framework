#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <flecs.h>

// Upgrades and molting. Upgrades are a string-keyed registry loaded from JSON: each changes one or more
// reflected stats on the player ("ClawSnap.radius", "PlayerStats.max_hp"), so a new upgrade that tunes
// an existing stat is a JSON entry with no C++. When the run's shells reach the molt threshold the run
// molts: the scene pauses and MoltOffer holds up to 3 distinct upgrades rolled from RngState. The player
// selects one (1/2/3 or a click on its card) and confirms it (Enter or the Confirm button), which applies
// it and resumes the run.
namespace crabnado {

inline constexpr int kMoltChoices = 3;

enum class UpgradeOp { Add, Multiply };

// One stat change: `member` of the player's component named `component`, a reflected float. An effect with a
// `label` is shown on the molt card as "label before » after", with `unit` after the numbers; one without (a
// heal that rides along with a max raise) isn't shown. `lower_is_better` marks a stat such as a cooldown, whose
// decrease the card shows as a gain.
struct UpgradeEffect {
	std::string component;
	std::string member;
	UpgradeOp op{UpgradeOp::Add};
	float value{0.0F};
	std::string label;
	std::string unit;
	bool lower_is_better{false};
};

// An eligibility condition: the player's reflected float `stat` ("Riptide.level") must be at least `min` and at
// most `max`, where set.
struct UpgradeRequirement {
	std::string component;
	std::string member;
	std::optional<float> min;
	std::optional<float> max;
};

struct Upgrade {
	std::string id;
	std::string name;
	std::string description;
	std::vector<UpgradeEffect> effects;
	std::optional<UpgradeRequirement> requires_;
	// The first molt offers one eligible flagged upgrade in its first slot.
	bool first_molt{false};
	// The card icon: a 96x96 image, as an asset path ("textures/crabnado/upgrades/scuttle.png"), and its
	// texture handle (-1 when it has none or it failed to load).
	std::string icon;
	int icon_texture{-1};
};

// Singleton: every upgrade, in id order.
struct UpgradeRegistry {
	std::vector<Upgrade> upgrades;
};

// Singleton: the upgrade ids offered by the current molt (unused slots are empty) and the slot the player has
// selected, waiting for confirmation (-1 when none).
struct MoltOffer {
	std::array<std::string, kMoltChoices> ids;
	int selected{-1};
};

// One labelled effect of an upgrade as the molt card shows it: the player's stat now and after the upgrade.
struct StatPreview {
	std::string label;
	std::string unit;
	float before{0.0F};
	float after{0.0F};
	bool lower_is_better{false};
};

// Registers MoltOffer (reflected) and UpgradeRegistry, loads the registry from `path` and its icons, and
// registers the MoltCheck (pausable) and MoltPick (runs while paused) systems. Call from the module constructor.
void RegisterUpgrades(const flecs::world& world, std::string_view path);

// The upgrade with `id`, or nullptr.
[[nodiscard]] const Upgrade* FindUpgrade(const flecs::world& world, std::string_view id);

// The upgrade's labelled effects with the player's current and resulting values, in effect order. Effects
// whose stat the player doesn't have are left out.
[[nodiscard]] std::vector<StatPreview> PreviewUpgrade(const flecs::world& world, const Upgrade& upgrade);

// Selects the offered upgrade in slot `choice` (0-based). Does nothing unless the run is molting and the slot
// holds an upgrade.
void SelectUpgrade(const flecs::world& world, int choice);

// Applies the selected upgrade (see ChooseUpgrade). Does nothing when none is selected.
void ConfirmUpgrade(const flecs::world& world);

// Applies the offered upgrade in slot `choice` (0-based) to the player and resumes the run. Does nothing
// unless the run is molting and the slot holds an upgrade.
void ChooseUpgrade(const flecs::world& world, int choice);

} // namespace crabnado
