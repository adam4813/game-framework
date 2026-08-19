#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/physics/physics_components.hpp"
#include "engine/physics/physics_types.hpp"

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

	/// Sync body from ECS to physics backend
	void SyncBodyToBackend(
		EntityId entity,
		const PhysicsTransform& transform,
		const RigidBody& body,
		const CollisionShape& shape
	);

	/// Sync body state from physics backend to ECS
	[[nodiscard]] PhysicsSyncResult SyncBodyFromBackend(EntityId entity) const;

	/// Remove body from backend
	void RemoveBody(EntityId entity);

	/// Check if backend has body
	[[nodiscard]] bool HasBody(EntityId entity) const;

	/// Apply force and torque to body
	void ApplyForce(EntityId entity, const glm::vec3& force, const glm::vec3& torque);

	/// Apply impulse to body
	void ApplyImpulse(EntityId entity, const glm::vec3& impulse, const glm::vec3& point);

	/// Set linear and angular velocity directly (physics module internal — consume PhysicsVelocityOverride).
	void SetBodyVelocity(EntityId entity, const glm::vec3& linear, const glm::vec3& angular);

	/// Get collision events from this frame
	[[nodiscard]] std::vector<CollisionInfo> GetCollisionEvents() const;

	/// Raycast query
	[[nodiscard]] std::optional<RaycastResult> Raycast(const Ray& ray) const;

	/// Raycast all hits
	[[nodiscard]] std::vector<RaycastResult> RaycastAll(const Ray& ray) const;

	/// Check if initialized
	[[nodiscard]] bool IsInitialized() const { return initialized_; }

#ifdef JPH_DEBUG_RENDERER
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

	// Entity to Jolt body mapping
	std::unordered_map<EntityId, uint32_t> entity_to_body_;

#ifdef JPH_DEBUG_RENDERER
	JPH::DebugRenderer* debug_renderer_ = nullptr;
#endif
};

} // namespace engine::physics
