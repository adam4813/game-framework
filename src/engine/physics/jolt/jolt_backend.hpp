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

	/// Create the entity's body, or bring an existing one up to date. `entity_name` only labels errors.
	/// - No body yet: creates one at `transform`. A shape that cannot be built (an unreadable mesh file, a mesh on a
	///   non-static body, a convex hull with no vertices) logs an error naming the entity and creates no body.
	/// - Same `body` and `shape` as last applied: does nothing, so repeated sets during a scene load are free.
	/// - A changed `shape`, or a static body becoming kinematic or dynamic: recreates the body where it is now, keeping
	///   its velocity. If the new shape cannot be built, the error is logged and the old body stays as it was.
	/// - Any other `body` change: updates the body in place.
	/// `transform` is ignored once the body exists; a teleport goes through `SetBodyTransform`.
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
	/// With `CharacterBody::inner_body`, Jolt also adds a kinematic inner body (the same capsule, on the character's
	/// layer, user data = entity) that follows every move, teleport and replayed step.
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

	/// Set a character's linear velocity. The next step keeps its part along up (unless grounded and not leaving the
	/// ground); `CharacterMove` replaces the planar part. ECS code restores a character for replay through
	/// `RestoreCharacter` (character_step.hpp), which also writes the state to its components.
	void SetCharacterVelocity(flecs::entity_t entity, const glm::vec3& velocity);

	/// Re-detect a character's contacts at its current position, so its ground state matches a position just set.
	/// Uses the step's temp allocator, so never call it while another thread is in the backend (character_step.hpp,
	/// "Where to call them").
	void RefreshCharacterContacts(flecs::entity_t entity);

	/// Advance one character by `delta_time`: planar velocity, gravity along up, grounding, slopes, steps. Call before
	/// `StepSimulation` in the same Tick. ECS code steps a character through `StepCharacterTick`
	/// (character_step.hpp), which reads its input and writes the result back.
	void StepCharacter(flecs::entity_t entity, float delta_time, const CharacterStepInput& input);

	/// The character's position and ground state as of the last step.
	[[nodiscard]] CharacterSyncResult GetCharacterSyncResult(flecs::entity_t entity) const;

	/// The entity position last written back to its Transform, for teleport detection.
	[[nodiscard]] glm::vec3 GetCharacterWrittenPosition(flecs::entity_t entity) const;
	void SetCharacterWrittenPosition(flecs::entity_t entity, const glm::vec3& position);

	/// Takes the collision events of every StepSimulation call since the last take, leaving none, so a Frame that ran
	/// several Ticks delivers them all and a Frame that ran none delivers nothing twice.
	[[nodiscard]] std::vector<CollisionInfo> TakeCollisionEvents();

	/// The closest hit; see `engine::physics::Raycast` (physics_queries.hpp), the public entry point, for the contract.
	[[nodiscard]] std::optional<RaycastResult> Raycast(const Ray& ray, const RaycastFilter& filter = {}) const;

	/// Every hit, one per body, sorted by distance; see `engine::physics::RaycastAll`.
	[[nodiscard]] std::vector<RaycastResult> RaycastAll(const Ray& ray, const RaycastFilter& filter = {}) const;

	/// Check if initialized
	[[nodiscard]] bool IsInitialized() const { return initialized_; }

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER
	/// Set debug renderer for drawing physics bodies
	void SetDebugRenderer(JPH::DebugRenderer* renderer);

	/// Draw all bodies using the debug renderer
	void DebugDrawBodies() const;
#endif

private:
	// True while a character or the world steps; a query made then hits nothing (Jolt's state is mid-update).
	[[nodiscard]] bool RejectQueryWhileStepping() const;

	PhysicsConfig config_;
	bool initialized_{false};
	mutable bool stepping_{false};

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

	// A Jolt body (its BodyID's index and sequence number) and the components it was last built or updated from, so a
	// repeated set is told apart from a change.
	struct BodyRecord {
		uint32_t id{};
		RigidBody body;
		CollisionShape shape;
	};

	// Entity to Jolt body mapping
	std::unordered_map<flecs::entity_t, BodyRecord> entity_to_body_;

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER
	JPH::DebugRenderer* debug_renderer_ = nullptr;
#endif
};

} // namespace engine::physics
