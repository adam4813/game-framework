#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <flecs.h>

// Upgrades and molting. Upgrades are a string-keyed registry loaded from JSON: each changes one or more
// reflected stats on the player ("ClawSnap.radius", "PlayerStats.max_hp"), so a new upgrade that tunes
// an existing stat is a JSON entry with no C++. When the run's shells reach the molt threshold the run
// molts: the scene pauses and MoltOffer holds up to 3 distinct upgrades rolled from RngState. Picking one
// (1/2/3 or a click) applies it and resumes the run.
namespace crabnado {

inline constexpr int kMoltChoices = 3;

enum class UpgradeOp { Add, Multiply };

// One stat change: `member` of the player's component named `component`, a reflected float.
struct UpgradeEffect {
	std::string component;
	std::string member;
	UpgradeOp op{UpgradeOp::Add};
	float value{0.0F};
};

struct Upgrade {
	std::string id;
	std::string name;
	std::string description;
	std::vector<UpgradeEffect> effects;
};

// Singleton: every upgrade, in id order.
struct UpgradeRegistry {
	std::vector<Upgrade> upgrades;
};

// Singleton: the upgrade ids offered by the current molt. Unused slots are empty.
struct MoltOffer {
	std::array<std::string, kMoltChoices> ids;
};

// Registers MoltOffer (reflected) and UpgradeRegistry, loads the registry from `path`, and registers the
// MoltCheck (pausable) and MoltPick (runs while paused) systems. Call from the module constructor.
void RegisterUpgrades(const flecs::world& world, std::string_view path);

// The upgrade with `id`, or nullptr.
[[nodiscard]] const Upgrade* FindUpgrade(const flecs::world& world, std::string_view id);

// Applies the offered upgrade in slot `choice` (0-based) to the player and resumes the run. Does nothing
// unless the run is molting and the slot holds an upgrade.
void ChooseUpgrade(const flecs::world& world, int choice);

} // namespace crabnado
