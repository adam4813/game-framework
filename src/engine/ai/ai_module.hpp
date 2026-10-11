#pragma once

#include <flecs.h>

namespace engine::ai {

/// AI module — umbrella that registers the shared Blackboard component (with reflection +
/// scripting) and imports all AI sub-modules (BTModule, GOAPModule, FSMModule, UtilityModule,
/// SteeringModule).
///
/// Sub-modules are independently importable for opt-in system registration:
///   world.import<ai::BTModule>();        // BT systems only
///   world.import<ai::FSMModule>();       // FSM only
///
/// For Blackboard reflection/scripting exposure, import this umbrella:
///   world.import<ai::AIModule>();        // all systems + registered Blackboard
class AIModule {
public:
	explicit AIModule(flecs::world& world);
};

} // namespace engine::ai
