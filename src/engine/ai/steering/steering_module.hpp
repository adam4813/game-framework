#pragma once

#include <flecs.h>

namespace engine::ai {

/// Steering behaviors module.
///
/// Registers: SteeringAgent, SteeringTarget components,
/// "SteeringSystem" (OnUpdate / Pausable).
/// Integrates velocity into spatial::Transform::position each frame.
///
/// JSON loaders (inline — all steering data is numeric):
///   ```json
///   "steering_agent": {
///     "max_speed": 5.0, "max_force": 10.0, "mass": 1.0,
///     "behaviors": [
///       { "type": "arrive", "weight": 1.0, "slow_radius": 3.0, "stop_radius": 0.15 },
///       { "type": "separation", "weight": 0.5, "neighbor_radius": 3.0 }
///     ]
///   }
///   "steering_target": [1.0, 0.0, 3.0]
///   ```
class SteeringModule {
public:
	explicit SteeringModule(const flecs::world& world);
};

} // namespace engine::ai
