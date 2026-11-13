#pragma once

#include <memory>
#include <string>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>

#include "physics_types.hpp"

namespace engine::physics {

// === PHYSICS COMPONENTS ===

/// Rigid body definition and behavior
struct RigidBody {
	MotionType motion_type{MotionType::Dynamic};
	float mass{1.0F};
	float linear_damping{0.05F};
	float angular_damping{0.05F};
	float friction{0.5F};
	float restitution{0.3F};
	bool enable_ccd{false};
	bool use_gravity{true};
	float gravity_scale{1.0F};
	CollisionLayer layer{CollisionLayer::Default};
	uint32_t collision_mask{0xFFFFFFFF};
};

/// Collision shape for physics body
struct CollisionShape {
	ShapeType type{ShapeType::Box};
	glm::vec3 box_half_extents{0.5F, 0.5F, 0.5F};
	float sphere_radius{0.5F};
	float capsule_radius{0.5F};
	float capsule_height{1.0F};
	float cylinder_radius{0.5F};
	float cylinder_height{1.0F};
	glm::vec3 offset{0.0F};
	glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
	std::vector<ChildShape> compound_children;
	/// Model file (.glb/.gltf) whose triangles make a static `Mesh` shape. Read only when `type == Mesh`.
	/// Level JSON resolves it against the asset directory; C++ and script authors pass a resolved path.
	std::string mesh_path;
};

/// Marks an entity as a walking character, paired with a capsule `CollisionShape` (an error without one, or with a
/// `RigidBody`). Backed by Jolt's `CharacterVirtual`. Defaults are Jolt's own.
struct CharacterBody {
	float max_slope_deg{50.0F};
	float step_height{0.4F};
	float stick_to_floor{0.5F};
	float mass{70.0F};
	/// Stored for authors; the backend has only moving and non-moving object layers, so it doesn't filter on this yet.
	CollisionLayer layer{CollisionLayer::Player};
};

/// Desired velocity, written by the game each frame. Only the part perpendicular to the character's up is used.
/// Without it the desired velocity is zero.
struct CharacterMove {
	glm::vec3 velocity{0.0F};
};

/// The single acceleration an entity feels, which also defines its up (-normalize(acceleration)). Without it an
/// entity uses `PhysicsWorldConfig::gravity`. Only characters read it for now.
struct Gravity {
	glm::vec3 acceleration{0.0F, -9.81F, 0.0F};
};

/// Runtime state of a character, written every frame for inspection.
struct CharacterState {
	bool grounded{false};
	flecs::entity_t ground_entity{0};
	glm::vec3 ground_normal{0.0F, 1.0F, 0.0F};
	glm::vec3 velocity{0.0F};
};

/// Velocity component tracked by physics (separate from engine::components::Velocity)
struct PhysicsVelocity {
	glm::vec3 linear{0.0F};
	glm::vec3 angular{0.0F};
};

/// Forces to apply to body (cleared each frame if clear_after_apply is true)
struct PhysicsForce {
	glm::vec3 force{0.0F};
	glm::vec3 torque{0.0F};
	bool clear_after_apply{true};
};

/// Impulse to apply immediately (consumed and removed after physics step)
struct PhysicsImpulse {
	glm::vec3 impulse{0.0F};
	glm::vec3 point{0.0F};
};

/// Direct velocity override (consumed and removed after physics step). Use to teleport a body's
/// velocity without applying a force or impulse — e.g. restoring saved state. The physics
/// module applies this to the Jolt body and removes the component the same frame.
struct PhysicsVelocityOverride {
	glm::vec3 linear{0.0F};
	glm::vec3 angular{0.0F};
};

/// Collision events for this entity (populated by physics system each frame)
struct CollisionEvents {
	std::vector<CollisionInfo> events;
};

// === TAG COMPONENTS ===

struct IsTrigger {};  // Body is a trigger volume
struct IsSleeping {}; // Body is sleeping

// === SINGLETON COMPONENTS ===

/// World-level physics configuration. Physics steps once per fixed Tick, so its rate is the fixed step's
/// (fixed_step::FixedStepConfig), not a field here.
struct PhysicsWorldConfig {
	glm::vec3 gravity{0.0F, -9.81F, 0.0F};
	bool enable_sleeping{true};
	bool show_debug_physics{false};
};

/// Backend pointer for external access (raycasting, etc.)
struct PhysicsBackendPtr {
	std::shared_ptr<class IPhysicsBackend> backend;
};

// === RELATIONSHIP COMPONENTS ===

/// Constraint between two bodies (used with flecs relationships)
struct PhysicsConstraint {
	ConstraintType type{ConstraintType::Fixed};
	glm::vec3 anchor_a{0.0F};
	glm::vec3 anchor_b{0.0F};
	glm::vec3 axis{0.0F, 1.0F, 0.0F};
	bool enable_limits{false};
	float min_limit{0.0F};
	float max_limit{0.0F};
};

} // namespace engine::physics
