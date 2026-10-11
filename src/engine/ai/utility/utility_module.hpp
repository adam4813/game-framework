#pragma once

#include <flecs.h>

namespace engine::ai {

/// Utility AI module.
///
/// Registers: UtilityAgent component, UtilityActionRegistry singleton,
/// "UtilityEval" + "UtilityExecute" systems (OnUpdate / Pausable).
///
/// JSON loaders:
///   Entity component — actions defined in a file, runtime config inline:
///   ```json
///   "utility_agent": { "file": "assets/ai/fighter_utility.json", "eval_interval": 0.25 }
///   ```
///
///   Utility definition file:
///   ```json
///   { "actions": [
///       { "name": "AttackPlayer", "cooldown": 1.0,
///         "considerations": [
///           { "context_key": "health",   "min": 0.0, "max": 100.0, "weight": 1.0 },
///           { "context_key": "distance", "min": 0.0, "max": 20.0,  "weight": 2.0 }
///         ] }
///   ] }
///   ```
class UtilityModule {
public:
	explicit UtilityModule(const flecs::world& world);
};

} // namespace engine::ai
