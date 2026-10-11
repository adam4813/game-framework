#pragma once

#include <flecs.h>

namespace engine::ai {

/// Behavior Tree module.
///
/// Registers: BehaviorTree component, BTHandlerRegistry singleton,
/// "BTExecution" system (OnUpdate / Pausable).
/// Import AIModule first (or separately) to get Blackboard registered.
///
/// JSON component loader — entity:
/// ```json
/// "behavior_tree": { "file": "assets/ai/patrol.json", "tick_interval": 0.0 }
/// ```
///
/// Tree definition file (engine/graph/graph_json.hpp format):
/// ```json
/// { "nodes": [
///     { "id": 1, "type": "sequence" },
///     { "id": 2, "parent": 1, "type": "condition", "handler": "CanSeeTarget" },
///     { "id": 3, "parent": 1, "type": "action",    "handler": "ChaseTarget" }
/// ] }
/// ```
class BTModule {
public:
	explicit BTModule(flecs::world& world);
};

} // namespace engine::ai
