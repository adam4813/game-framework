#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/physics/physics_components.hpp"
#include "engine/physics/physics_types.hpp"
#include "jolt_debug_config.hpp"

// Forward declarations for Jolt
namespace JPH {
class PhysicsSystem;
class TempAllocatorImpl;
class JobSystemThreadPool;
class BroadPhaseLayerInterface;
class ObjectVsBroadPhaseLayerFilter;
class ObjectLayerPairFilter;
class BodyID;
class ContactListener;
class DebugRenderer;
} // namespace JPH

namespace engine::physics {

struct CharacterRecord; // defined in jolt_backend.cpp
struct MeshShapeCache;  // defined in jolt_backend.cpp so this header needs no Jolt shape types

/// Concrete Jolt Physics System implementation
class JoltBackend {
public:
	explicit JoltBackend(const PhysicsConfig& config);
	~JoltBackend();

	JoltBackend(const JoltBackend&) = delete;
	JoltBackend& operator=(const JoltBackend&) = delete;
	JoltBackend(JoltBackend&&) = delete;
	JoltBackend& operator=(JoltBackend&&) = delete;

	/// Initialize the physics system
	bool Initialize();

	/// Shutdown the physics system
	void Shutdown();

	/// Set world gravity
	void SetGravity(const glm::vec3& gravity) const;

	/// Get world gravity
	[[nodiscard]] glm::vec3 GetGravity() const;

	/// Step the physics simulation
	void StepSimulation(float delta_time) const;

	/// Sync body from ECS to physics backend. `entity_name` only labels errors. A shape that cannot be built (an
	/// unreadable mesh file, a mesh on a non-static body, a convex hull with no vertices) logs an error naming the
	/// entity and creates no body.
	void SyncBodyToBackend(
		flecs::entity_t entity,
		std::string_view entity_name,
		const PhysicsTransform& transform,
		const RigidBody& body,
		const CollisionShape& shape
	);

	void SetBodyTransform(flecs::entity_t entity, const PhysicsTransform& transform);

	/// Sync body state from physics backend to ECS
	[[nodiscard]] PhysicsSyncResult GetPhysicsSyncResult(flecs::entity_t entity) const;

	/// Remove body from backend
	void RemoveBody(flecs::entity_t entity);

	/// Check if backend has body
	[[nodiscard]] bool HasBody(flecs::entity_t entity) const;

	/// Apply force and torque to body
	void ApplyForce(flecs::entity_t entity, const glm::vec3& force, const glm::vec3& torque);

	/// Apply impulse to body
	void ApplyImpulse(flecs::entity_t entity, const glm::vec3& impulse, const glm::vec3& point);

	/// Set linear and angular velocity directly (physics module internal — consume PhysicsVelocityOverride).
	void SetBodyVelocity(flecs::entity_t entity, const glm::vec3& linear, const glm::vec3& angular);

	/// Create a character from a capsule `CollisionShape`. Logs an error naming `entity_name` and creates nothing when
	/// the shape isn't a capsule. `position` is the entity's position (the capsule sits at position + shape offset).
	void AddCharacter(
		flecs::entity_t entity,
		std::string_view entity_name,
		const glm::vec3& position,
		const CharacterBody& body,
		const CollisionShape& shape
	);

	void RemoveCharacter(flecs::entity_t entity);

	[[nodiscard]] bool HasCharacter(flecs::entity_t entity) const;

	/// Teleport a character to an entity position.
	void SetCharacterPosition(flecs::entity_t entity, const glm::vec3& position);

	/// Advance one character by `delta_time`: planar velocity, gravity along up, grounding, slopes, steps. Call before
	/// `StepSimulation` for the same substep.
	void StepCharacter(flecs::entity_t entity, float delta_time, const CharacterStepInput& input);

	/// The character's position and ground state as of the last step.
	[[nodiscard]] CharacterSyncResult GetCharacterSyncResult(flecs::entity_t entity) const;

	/// The entity position last written back to its Transform, for teleport detection.
	[[nodiscard]] glm::vec3 GetCharacterWrittenPosition(flecs::entity_t entity) const;
	void SetCharacterWrittenPosition(flecs::entity_t entity, const glm::vec3& position);

	/// Get collision events from this frame
	[[nodiscard]] std::vector<CollisionInfo> GetCollisionEvents() const;

	/// Raycast query
	[[nodiscard]] std::optional<RaycastResult> Raycast(const Ray& ray) const;

	/// Raycast all hits
	[[nodiscard]] std::vector<RaycastResult> RaycastAll(const Ray& ray) const;

	/// Check if initialized
	[[nodiscard]] bool IsInitialized() const { return initialized_; }

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER
	/// Set debug renderer for drawing physics bodies
	void SetDebugRenderer(JPH::DebugRenderer* renderer);

	/// Draw all bodies using the debug renderer
	void DebugDrawBodies() const;
#endif

private:
	PhysicsConfig config_;
	bool initialized_{false};

	// Jolt objects
	std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator_;
	std::unique_ptr<JPH::JobSystemThreadPool> job_system_;
	std::unique_ptr<JPH::BroadPhaseLayerInterface> broad_phase_layer_interface_;
	std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> object_vs_broad_phase_layer_filter_;
	std::unique_ptr<JPH::ObjectLayerPairFilter> object_layer_pair_filter_;
	std::unique_ptr<JPH::PhysicsSystem> physics_system_;
	std::unique_ptr<JPH::ContactListener> contact_listener_;

	// Built mesh shapes by resolved path, so entities sharing a model file share one shape. Cleared on Shutdown.
	std::unique_ptr<MeshShapeCache> mesh_shape_cache_;

	// Entity to character mapping; characters are not bodies in the physics system.
	std::unordered_map<flecs::entity_t, std::unique_ptr<CharacterRecord>> characters_;

	// Entity to Jolt body mapping
	std::unordered_map<flecs::entity_t, uint32_t> entity_to_body_;

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER
	JPH::DebugRenderer* debug_renderer_ = nullptr;
#endif
};

} // namespace engine::physics
