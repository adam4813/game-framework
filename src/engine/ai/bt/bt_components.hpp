#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <flecs.h>

#include "../ai_components.hpp"
#include "engine/graph/graph.hpp"

namespace engine::ai {

enum class BTStatus : uint8_t { Invalid, Running, Success, Failure };

enum class BTNodeType : uint8_t {
	// Composites
	Selector, ///< OR — returns Success when any child succeeds
	Sequence, ///< AND — returns Success only when all children succeed
	Parallel, ///< Runs all children each tick; configurable success threshold
	// Decorators
	Inverter,  ///< Flips the child's result (Success↔Failure; Running passes through)
	Repeater,  ///< Repeats the child N times (-1 = infinite) until it fails
	Succeeder, ///< Always returns Success regardless of the child's result
	// Leaves
	Action,    ///< Invokes a named handler from BTHandlerRegistry::actions
	Condition, ///< Tests a named predicate from BTHandlerRegistry::conditions
};

/// Authored definition of a single Behavior Tree node.
struct BTNodeDef {
	BTNodeType type{BTNodeType::Action};
	std::string handler_key;     ///< Registry key for Action and Condition leaves
	int parallel_success_min{1}; ///< Parallel: how many children must succeed
	int repeat_count{-1};        ///< Repeater: max iterations (-1 = infinite)
};

/// Behavior Tree definition + per-entity runtime state.
/// Attach alongside a Blackboard (from ai_components.hpp).
struct BehaviorTree {
	graph::Tree<BTNodeDef> tree;
	/// Per-node status from the last tick, indexed by NodeId (size = node_count+1).
	/// Resized automatically on first tick.
	std::vector<BTStatus> node_statuses;
	BTStatus root_status{BTStatus::Invalid};
	float tick_interval{0.0F}; ///< Minimum seconds between ticks (0 = every frame)
	float tick_timer{0.0F};
};

// ── Singleton registries ────────────────────────────────────────────────────

/// Called for Action leaves. Return the node's new status.
using BTActionHandler = std::function<BTStatus(flecs::entity, Blackboard&, float /*dt*/)>;
/// Called for Condition leaves. Return true → Success, false → Failure.
using BTConditionHandler = std::function<bool(flecs::entity, const Blackboard&)>;

/// Singleton: maps string keys to BT leaf handlers.
/// Register handlers before entities with BehaviorTree are created.
struct BTHandlerRegistry {
	std::unordered_map<std::string, BTActionHandler> actions;
	std::unordered_map<std::string, BTConditionHandler> conditions;
};

} // namespace engine::ai
