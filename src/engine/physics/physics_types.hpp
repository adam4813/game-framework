#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine::physics {

// === PHYSICS TYPES & ENUMS ===

enum class MotionType { Static, Kinematic, Dynamic };

enum class ShapeType { Box, Sphere, Capsule, Cylinder, ConvexHull, Mesh, Compound };

/// The layer a body or character sits on. Raycasts filter on it (RaycastFilter::layer_mask); body-vs-body contacts
/// don't. The default `int` underlying type keeps it reflectable (see the flecs-systems skill).
enum class CollisionLayer { Default = 0, Player = 1, Enemy = 2, Obstacle = 3, Trigger = 4 };

/// Every layer with its names: `json` in level files, `reflected` for the Explorer (and `Layer_<reflected>` in scripts).
/// The one list the loader, reflection and script constants read, so a new layer is one entry here.
struct CollisionLayerName {
	const char* json;
	const char* reflected;
	CollisionLayer layer;
};
inline constexpr std::array kCollisionLayerNames{
	CollisionLayerName{.json = "default", .reflected = "Default", .layer = CollisionLayer::Default},
	CollisionLayerName{.json = "player", .reflected = "Player", .layer = CollisionLayer::Player},
	CollisionLayerName{.json = "enemy", .reflected = "Enemy", .layer = CollisionLayer::Enemy},
	CollisionLayerName{.json = "obstacle", .reflected = "Obstacle", .layer = CollisionLayer::Obstacle},
	CollisionLayerName{.json = "trigger", .reflected = "Trigger", .layer = CollisionLayer::Trigger},
};

inline constexpr auto kCollisionLayerCount = static_cast<uint32_t>(kCollisionLayerNames.size());

/// The `RaycastFilter::layer_mask` bit for one layer.
constexpr uint32_t LayerBit(const CollisionLayer layer) { return 1U << static_cast<uint32_t>(layer); }

inline constexpr uint32_t kAllLayersMask = (1U << kCollisionLayerCount) - 1U;
/// What a query hits when it doesn't say: every layer but `Trigger`.
inline constexpr uint32_t kDefaultRaycastMask = kAllLayersMask & ~LayerBit(CollisionLayer::Trigger);

enum class ConstraintType : uint8_t { Fixed, Hinge, Slider, Ball, Distance };

// Physics configuration
struct PhysicsConfig {
	glm::vec3 gravity{0.0F, -9.81F, 0.0F};
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

	bool operator==(const ChildShape&) const = default;
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
	flecs::entity_t entity_a{0};
	flecs::entity_t entity_b{0};
	std::vector<ContactPoint> contacts;
};

// Physics simulation results
struct PhysicsSyncResult {
	glm::vec3 position{0.0F};
	glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
	glm::vec3 linear_velocity{0.0F};
	glm::vec3 angular_velocity{0.0F};
};

// What a character is asked to do for one substep.
struct CharacterStepInput {
	/// Desired velocity; any part along up is discarded.
	glm::vec3 move_velocity{0.0F};
	/// Acceleration the character feels; its up is -normalize(gravity).
	glm::vec3 gravity{0.0F, -9.81F, 0.0F};
};

// A character's state after stepping. `position` is the entity position (the capsule center minus the shape
// offset), i.e. what belongs in the entity's Transform.
struct CharacterSyncResult {
	glm::vec3 position{0.0F};
	bool grounded{false};
	flecs::entity_t ground_entity{0};
	glm::vec3 ground_normal{0.0F, 1.0F, 0.0F};
	glm::vec3 velocity{0.0F};
};

/// A ray for a query. `direction` need not be normalized; a zero direction or `max_distance <= 0` hits nothing.
struct Ray {
	glm::vec3 origin{0.0F};
	glm::vec3 direction{0.0F, 0.0F, 1.0F};
	float max_distance{100.0F};
};

/// What a query may hit. Nothing is ignored implicitly, not even the caller: pass its entity in `ignore`.
struct RaycastFilter {
	/// `LayerBit`s of the layers to hit.
	uint32_t layer_mask{kDefaultRaycastMask};
	/// Entities whose bodies (or character inner bodies) the ray passes through. Viewed, not copied.
	std::span<const flecs::entity_t> ignore{};
};

/// One hit: the entity whose body was hit, where, the surface normal there, and how far along the ray.
struct RaycastResult {
	flecs::entity_t entity{0};
	glm::vec3 hit_point{0.0F};
	glm::vec3 normal{0.0F};
	float distance{0.0F};
};

// Constraint configuration
struct ConstraintConfig {
	flecs::entity_t entity_a{0};
	flecs::entity_t entity_b{0};
	ConstraintType type{ConstraintType::Fixed};
	glm::vec3 anchor_a{0.0F};
	glm::vec3 anchor_b{0.0F};
	glm::vec3 axis{0.0F, 1.0F, 0.0F};
	bool enable_limits{false};
	float min_limit{0.0F};
	float max_limit{0.0F};
};

} // namespace engine::physics
