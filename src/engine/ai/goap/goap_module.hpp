#pragma once

#include <flecs.h>

namespace engine::ai {

/// GOAP (Goal-Oriented Action Planning) module.
///
/// Registers: GOAPAgent, GOAPActionRegistry singleton,
/// "GOAPPlanner" + "GOAPExecutor" systems (OnUpdate / Pausable).
///
/// JSON loaders:
///   Entity component — inline world_state, goals referencing an actions file:
///   ```json
///   "goap_agent": {
///     "actions_file": "assets/ai/soldier_actions.json",
///     "world_state":  { "has_weapon": false },
///     "goals": [{ "name": "BeArmed", "desired_state": [{"key":"has_weapon","value":true}], "priority": 1.0 }]
///   }
///   ```
///
///   Actions file format:
///   ```json
///   { "actions": [
///       { "name": "PickUpWeapon", "cost": 1.0, "duration": 0.0,
///         "preconditions": [{"key": "has_weapon", "value": false}],
///         "effects":       [{"key": "has_weapon", "value": true }] }
///   ] }
///   ```
class GOAPModule {
public:
	explicit GOAPModule(const flecs::world& world);
};

} // namespace engine::ai
