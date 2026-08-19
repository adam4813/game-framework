#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine::physics {

// === PHYSICS TYPES & ENUMS ===

using EntityId = uint32_t;

enum class MotionType : uint8_t { Static, Kinematic, Dynamic };

enum class ShapeType : uint8_t { Box, Sphere, Capsule, Cylinder, ConvexHull, Mesh, Compound };

enum class CollisionLayer : uint8_t { Default = 0, Player = 1, Enemy = 2, Obstacle = 3, Trigger = 4 };

enum class ConstraintType : uint8_t { Fixed, Hinge, Slider, Ball, Distance };

// Physics configuration
struct PhysicsConfig {
	glm::vec3 gravity{0.0F, -9.81F, 0.0F};
	float fixed_timestep{1.0F / 60.0F};
	int collision_steps{1};
	bool enable_sleeping{true};
};

// Transform for physics synchronization
struct PhysicsTransform {
	glm::vec3 position{0.0F};
	glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
	glm::vec3 scale{1.0F};

	static PhysicsTransform FromMatrix(const glm::mat4& matrix) {
		PhysicsTransform result;
		result.position = glm::vec3(matrix[3]);

		const glm::vec3 col0(matrix[0]);
		const glm::vec3 col1(matrix[1]);
		const glm::vec3 col2(matrix[2]);
		result.scale = glm::vec3(glm::length(col0), glm::length(col1), glm::length(col2));

		if (constexpr float k_epsilon = 1e-6F;
			result.scale.x > k_epsilon && result.scale.y > k_epsilon && result.scale.z > k_epsilon) {
			const glm::mat3 rot_mat(col0 / result.scale.x, col1 / result.scale.y, col2 / result.scale.z);
			result.rotation = glm::quat_cast(rot_mat);
		}

		return result;
	}
};

// Shape configuration for backend
struct ChildShape {
	ShapeType type{ShapeType::Box};
	glm::vec3 position{0.0F};
	glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
	glm::vec3 box_half_extents{0.5F, 0.5F, 0.5F};
	float sphere_radius{0.5F};
	float capsule_radius{0.5F};
	float capsule_height{1.0F};
	float cylinder_radius{0.5F};
	float cylinder_height{1.0F};
};

struct ShapeConfig {
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
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> indices;
};

// Collision information
struct ContactPoint {
	glm::vec3 position{0.0F};
	glm::vec3 normal{0.0F};
	float penetration_depth{0.0F};
};

struct CollisionInfo {
	EntityId entity_a{0};
	EntityId entity_b{0};
	std::vector<ContactPoint> contacts;
};

// Physics simulation results
struct PhysicsSyncResult {
	glm::vec3 position{0.0F};
	glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
	glm::vec3 linear_velocity{0.0F};
	glm::vec3 angular_velocity{0.0F};
};

// Raycast
struct Ray {
	glm::vec3 origin{0.0F};
	glm::vec3 direction{0.0F, 0.0F, 1.0F};
	float max_distance{100.0F};
};

struct RaycastResult {
	EntityId entity{0};
	glm::vec3 hit_point{0.0F};
	float distance{0.0F};
};

// Constraint configuration
struct ConstraintConfig {
	EntityId entity_a{0};
	EntityId entity_b{0};
	ConstraintType type{ConstraintType::Fixed};
	glm::vec3 anchor_a{0.0F};
	glm::vec3 anchor_b{0.0F};
	glm::vec3 axis{0.0F, 1.0F, 0.0F};
	bool enable_limits{false};
	float min_limit{0.0F};
	float max_limit{0.0F};
};

} // namespace engine::physics
