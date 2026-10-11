#pragma once

#include <flecs.h>

namespace engine::ai {

/// FSM (Finite State Machine) module.
///
/// Registers: FSMAgent component, "FSMUpdate" system (OnUpdate / Pausable).
///
/// JSON loaders:
///   Entity component — states defined in a separate file:
///   ```json
///   "fsm_agent": { "file": "assets/ai/guard_fsm.json", "initial_state": "Patrol" }
///   ```
///
///   FSM definition file:
///   ```json
///   { "states": [
///       { "name": "Patrol",
///         "transitions": [{ "condition": "enemy_visible", "expected": true, "target": "Chase", "min_time": 0.5 }] },
///       { "name": "Chase", "max_duration": 10.0, "timeout_target": "Patrol" }
///   ] }
///   ```
class FSMModule {
public:
	explicit FSMModule(const flecs::world& world);
};

} // namespace engine::ai
