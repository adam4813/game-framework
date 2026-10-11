#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <flecs.h>

namespace engine::ai {

/// Maps a context value to a [0,1] utility score via a linear range + power-curve weight.
struct UtilityConsideration {
	std::string context_key; ///< Key in UtilityAgent::context
	float min_value{0.0F};   ///< Raw value mapped to 0
	float max_value{1.0F};   ///< Raw value mapped to 1
	float weight{1.0F};      ///< Exponent applied to the normalized value (response curve)
};

/// Definition of a scored action the Utility AI can select.
struct UtilityActionDef {
	std::string name;
	std::vector<UtilityConsideration> considerations; ///< Score = product of all normalized values
	float cooldown{0.0F};                             ///< Minimum seconds before re-selection
};

/// Per-entity Utility AI agent.
/// Populate actions and seed context values before ticking.
struct UtilityAgent {
	std::vector<UtilityActionDef> actions;
	std::string current_action;
	std::unordered_map<std::string, float> context;   ///< Writable context blackboard
	std::unordered_map<std::string, float> cooldowns; ///< action name → remaining cooldown
	float eval_interval{0.25F};                       ///< Re-score actions every N seconds (0 = every frame)
	float eval_timer{0.0F};
};

// ── Singleton registry ──────────────────────────────────────────────────────

/// Called every frame while the selected action is active.
using UtilityActionExecutor = std::function<void(flecs::entity, UtilityAgent&, float /*dt*/)>;

/// Singleton: maps action names to executors.
/// Register handlers before entities with UtilityAgent are created.
struct UtilityActionRegistry {
	std::unordered_map<std::string, UtilityActionExecutor> handlers;
};

} // namespace engine::ai
