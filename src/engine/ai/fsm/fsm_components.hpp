#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace engine::ai {

/// A condition checked against the FSM agent's condition table.
struct FSMTransition {
	std::string condition; ///< Key in FSMAgent::conditions
	bool expected{true};   ///< Fire when condition value equals expected
	std::string target;    ///< Name of the state to enter
	float min_time{0.0F};  ///< Minimum time (seconds) in current state before this fires
};

/// Definition of a single FSM state.
struct FSMStateDef {
	std::string name;
	std::vector<FSMTransition> transitions;
	float max_duration{-1.0F};  ///< Auto-transition after N seconds (-1 = disabled)
	std::string timeout_target; ///< State to enter on timeout (empty = stay)
};

/// Per-entity Finite State Machine agent.
/// Populate states and set current_state before ticking.
struct FSMAgent {
	std::vector<FSMStateDef> states;
	std::string current_state;
	std::string previous_state;
	float state_elapsed{0.0F};                        ///< Seconds spent in current state
	std::unordered_map<std::string, bool> conditions; ///< Writable condition table
	bool state_changed{false};                        ///< True for exactly one frame after a transition fires
};

} // namespace engine::ai
