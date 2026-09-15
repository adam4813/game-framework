#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <flecs.h>

namespace engine::ai {

/// A single boolean fact in the agent's perceived world model.
struct GOAPFact {
	std::string key;
	bool value{true};
};

/// An action the GOAP planner may sequence.
struct GOAPActionDef {
	std::string name;
	std::vector<GOAPFact> preconditions; ///< World-state facts required before execution
	std::vector<GOAPFact> effects;       ///< Facts set in world state upon completion
	float cost{1.0F};                    ///< Planner path cost (lower = preferred)
	float duration{0.0F};                ///< Execution time in seconds (0 = instantaneous)
};

/// A goal the agent wants to achieve.
struct GOAPGoal {
	std::string name;
	std::vector<GOAPFact> desired_state;
	float priority{1.0F}; ///< Higher priority goals are planned first
};

/// Per-entity GOAP agent state.
/// Populate available_actions, goals, and initial world_state before ticking.
struct GOAPAgent {
	std::unordered_map<std::string, bool> world_state; ///< Current perceived facts
	std::vector<GOAPActionDef> available_actions;
	std::vector<GOAPGoal> goals;
	std::vector<std::string> current_plan; ///< Ordered action names from the planner
	size_t current_action_index{0};
	float action_elapsed{0.0F};
	bool needs_replan{true}; ///< Set to true to trigger a new planning pass
	bool plan_failed{false}; ///< Set by system when no plan can be found
};

// ── Singleton registry ──────────────────────────────────────────────────────

/// Called when an action starts. Return false to abort immediately.
using GOAPActionStart = std::function<bool(flecs::entity, GOAPAgent&)>;
/// Called every frame while an action is executing. Return true when complete.
using GOAPActionTick = std::function<bool(flecs::entity, GOAPAgent&, float /*dt*/)>;

/// Singleton: maps action names to start/tick handlers.
/// Register handlers before entities with GOAPAgent are created.
struct GOAPActionRegistry {
	struct Entry {
		GOAPActionStart on_start;
		GOAPActionTick on_tick;
	};
	std::unordered_map<std::string, Entry> handlers;
};

} // namespace engine::ai
