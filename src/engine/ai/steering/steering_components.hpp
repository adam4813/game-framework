#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace engine::ai {

enum class SteeringBehaviorType : uint8_t {
	Seek,   ///< Accelerate toward SteeringTarget::position
	Flee,   ///< Accelerate away from SteeringTarget::position
	Arrive, ///< Seek with deceleration as the agent enters slow_radius
	Wander, ///< Periodic directional drift using a projected wander sphere (deterministic)
	// Flocking — require neighbor data from the spatial module; see ai_module.cpp
	Separation, ///< Steer away from nearby SteeringAgent entities
	Cohesion,   ///< Steer toward the centroid of nearby SteeringAgent entities
	Alignment,  ///< Match velocity with nearby SteeringAgent entities
};

/// A single autonomous movement behavior with its own tuning parameters.
struct SteeringBehavior {
	SteeringBehaviorType type{SteeringBehaviorType::Seek};
	float weight{1.0F};          ///< Contribution weight when blending forces
	float slow_radius{3.0F};     ///< Arrive: begin decelerating within this radius
	float stop_radius{0.15F};    ///< Arrive: stop within this radius
	float wander_jitter{0.5F};   ///< Wander: angular rotation speed (radians/sec) — deterministic, not random
	float wander_radius{1.0F};   ///< Wander: radius of the projected wander sphere
	float wander_distance{2.0F}; ///< Wander: distance to project the wander sphere ahead
	float neighbor_radius{3.0F}; ///< Separation/Cohesion/Alignment: neighbor search radius
};

/// Attach to an entity to enable autonomous movement via the SteeringSystem.
/// The system writes steering_output each frame; game code (or a follow-up system)
/// should integrate velocity and apply it to spatial::Transform::position.
struct SteeringAgent {
	std::vector<SteeringBehavior> behaviors;
	glm::vec3 velocity{0.0F};
	float max_speed{5.0F};
	float max_force{10.0F};
	float mass{1.0F};
	glm::vec3 steering_output{0.0F};           ///< Blended force computed by SteeringSystem
	glm::vec3 wander_target{0.0F, 0.0F, 1.0F}; ///< Internal wander state (unit sphere)
};

/// Seek/flee/arrive target for a SteeringAgent.
/// Game code updates position each frame (or use the WorldTransform of a target entity).
struct SteeringTarget {
	glm::vec3 position{0.0F};
};

} // namespace engine::ai
