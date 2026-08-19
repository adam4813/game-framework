#include "physics_module.hpp"

#include <spdlog/spdlog.h>

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/ecs/ecs.hpp"
#include "engine/input/input.hpp"
#include "engine/platform/platform.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"
#include "jolt/jolt_backend.hpp"
#include "jolt/jolt_debug_renderer.hpp"
#include "physics_components.hpp"

namespace engine::physics {

namespace {
struct PhysicsAccumulator {
	float accumulated_time{0.0F};
};
} // namespace

PhysicsModule::PhysicsModule(const flecs::world& world) {
	// Initialize Jolt backend
	world.emplace<JoltBackend>(PhysicsConfig{});
	auto& physics = world.get_mut<JoltBackend>();
	if (!physics.Initialize()) {
		spdlog::error("[PhysicsModule] Failed to initialize Jolt");
		return;
	}

	// Ensure PhysicsWorldConfig singleton exists
	if (!world.has<PhysicsWorldConfig>()) {
		world.set<PhysicsWorldConfig>({});
	}

	// Register physics components with Flecs meta and expose them to the scripting system.
	// RegisterComponentForScripts is a no-op when no scripting backend is present, so this
	// keeps the physics module self-contained without hard-depending on scripting.
	world.component<PhysicsImpulse>().member<glm::vec3>("impulse").member<glm::vec3>("point");
	scripting::RegisterComponentForScripts(world, world.component<PhysicsImpulse>());

	// Partial meta reflection for the remaining physics components — visible in the Flecs
	// Explorer and available for future script exposure. CollisionShape is NOT script-exposed
	// because it owns a std::vector (not trivially copyable). RigidBody is also not exposed
	// to scripts; its enum and vector members make the binding non-trivial for the current
	// scripting backend. PhysicsVelocity and PhysicsForce are trivially copyable and safe.
	world.component<RigidBody>()
		.member("mass", &RigidBody::mass)
		.member("friction", &RigidBody::friction)
		.member("restitution", &RigidBody::restitution)
		.member("use_gravity", &RigidBody::use_gravity)
		.member("gravity_scale", &RigidBody::gravity_scale);

	world.component<CollisionShape>().member("offset", &CollisionShape::offset);

	world.component<PhysicsVelocity>()
		.member("linear", &PhysicsVelocity::linear)
		.member("angular", &PhysicsVelocity::angular);
	scripting::RegisterComponentForScripts(world, world.component<PhysicsVelocity>());

	world.component<PhysicsForce>()
		.member("force", &PhysicsForce::force)
		.member("torque", &PhysicsForce::torque)
		.member("clear_after_apply", &PhysicsForce::clear_after_apply);
	scripting::RegisterComponentForScripts(world, world.component<PhysicsForce>());

	// Trivially copyable state-restore hook — reflected and script-exposed like its sibling
	// request components so it can be authored from the Explorer / scripts.
	world.component<PhysicsVelocityOverride>()
		.member("linear", &PhysicsVelocityOverride::linear)
		.member("angular", &PhysicsVelocityOverride::angular);
	scripting::RegisterComponentForScripts(world, world.component<PhysicsVelocityOverride>());

	// Initialize accumulator for fixed timestep
	world.set<PhysicsAccumulator>({});

	// Initialize debug renderer if platform is available
#ifdef JPH_DEBUG_RENDERER
	std::shared_ptr<JoltDebugRendererAdapter> debug_renderer;
	auto& platform = world.get_mut<platform::PlatformRef>();
	debug_renderer = std::make_shared<JoltDebugRendererAdapter>(world, platform.ptr);
	physics.SetDebugRenderer(debug_renderer.get());
	world.set<std::shared_ptr<JoltDebugRendererAdapter>>(debug_renderer);
#endif

	// === SYSTEM: Apply forces each frame ===
	// Runs once per entity that has PhysicsForce
	world.system<const PhysicsForce>("PhysicsApplyForces")
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity& e, const PhysicsForce& force) {
			const auto world = e.world();
			if (auto& sys = world.get_mut<JoltBackend>(); sys.HasBody(e.id())) {
				sys.ApplyForce(e.id(), force.force, force.torque);
			}
			if (force.clear_after_apply) {
				if (!e.remove<PhysicsForce>()) {
					spdlog::warn("[PhysicsApplyForces] Failed to remove PhysicsForce from entity {}", e.id());
				}
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// === SYSTEM: Apply impulses each frame ===
	// Impulses are single-use and removed after application
	world.system<const PhysicsImpulse>("PhysicsApplyImpulses")
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity& e, const PhysicsImpulse& impulse) {
			const auto world = e.world();
			if (auto& sys = world.get_mut<JoltBackend>(); sys.HasBody(e.id())) {
				sys.ApplyImpulse(e.id(), impulse.impulse, impulse.point);
			}
			if (!e.remove<PhysicsImpulse>()) {
				spdlog::warn("[PhysicsApplyImpulses] Failed to remove PhysicsImpulse from entity {}", e.id());
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// === SYSTEM: Apply velocity overrides each frame ===
	// Single-use: sets the body's velocity directly and removes the component. Used for state
	// restore (e.g. loading a save snapshot) where you need to land at an exact velocity rather
	// than accumulate forces/impulses.
	world.system<const PhysicsVelocityOverride>("PhysicsApplyVelocityOverrides")
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity& e, const PhysicsVelocityOverride& vov) {
			const auto world = e.world();
			if (auto& sys = world.get_mut<JoltBackend>(); sys.HasBody(e.id())) {
				sys.SetBodyVelocity(e.id(), vov.linear, vov.angular);
			}
			e.remove<PhysicsVelocityOverride>();
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// === SYSTEM: Step physics simulation ===
	// Handles fixed timestep accumulation and physics stepping
	world.system("PhysicsStep")
		.kind(flecs::OnUpdate)
		.run([](const flecs::iter& it) {
			const auto world = it.world();
			auto& sys = world.get_mut<JoltBackend>();

			auto& [accumulated_time] = world.get_mut<PhysicsAccumulator>();
			const auto& cfg = world.get<PhysicsWorldConfig>();

			sys.SetGravity(cfg.gravity);

			accumulated_time += it.delta_time();

			int steps = 0;
			while (accumulated_time >= cfg.fixed_timestep && steps < cfg.max_substeps) {
				sys.StepSimulation(cfg.fixed_timestep);
				accumulated_time -= cfg.fixed_timestep;
				++steps;
			}

			// Clamp to prevent spiral of death
			accumulated_time = std::min(accumulated_time, cfg.fixed_timestep * static_cast<float>(cfg.max_substeps));
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// === OBSERVER: Sync physics bodies to backend on component set ===
	// Triggered when RigidBody, CollisionShape, or WorldTransform changes
	world.observer<const ecs::WorldTransform, const RigidBody, const CollisionShape>("PhysicsSyncToBackend")
		.event(flecs::OnSet)
		.each([](const flecs::entity& e, const ecs::WorldTransform& wt, const RigidBody& rb, const CollisionShape& cs) {
			auto& sys = e.world().get_mut<JoltBackend>();

			// Skip dynamic bodies already tracked by the backend. ComponentGetRefGeneric fires
			// modified() on every script handle read (see its trade-off comment); re-syncing a
			// live dynamic body would recreate its shape and call SetPositionAndRotation every
			// tick, preventing sleep and disrupting the simulation.
			// Kinematic/Static bodies are always re-synced so game code can reposition them.
			if (sys.HasBody(e.id()) && rb.motion_type == MotionType::Dynamic) {
				return;
			}

			PhysicsTransform phys_transform = PhysicsTransform::FromMatrix(wt.matrix);
			sys.SyncBodyToBackend(e.id(), phys_transform, rb, cs);

			// Add PhysicsVelocity if not present (for dynamic bodies)
			if (rb.motion_type == MotionType::Dynamic && !e.has<PhysicsVelocity>()) {
				e.set<PhysicsVelocity>({});
			}
		});

	// === OBSERVER: Remove bodies from backend on RigidBody removal ===
	world.observer<const RigidBody>("PhysicsRemoveBody")
		.event(flecs::OnRemove)
		.each([](const flecs::entity& e, const RigidBody&) {
			auto& sys = e.world().get_mut<JoltBackend>();
			sys.RemoveBody(e.id());
		});

	// === SYSTEM: Sync physics state back to ECS ===
	// Updates WorldTransform and PhysicsVelocity from Jolt simulation results
	world.system<ecs::WorldTransform, PhysicsVelocity, const RigidBody>("PhysicsSyncFromBackend")
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity& e, ecs::WorldTransform& wt, PhysicsVelocity& v, const RigidBody& rb) {
			if (rb.motion_type != MotionType::Dynamic) return;

			const auto& sys = e.world().get<JoltBackend>();
			if (!sys.HasBody(e.id())) return;

			auto [pos, rot, lin_vel, ang_vel] = sys.SyncBodyFromBackend(e.id());

			// Physics owns WorldTransform — write world-space values directly
			wt.position = pos;
			wt.rotation = glm::eulerAngles(rot);
			// Preserve scale from TransformPropagation
			wt.ComputeMatrix();

			v.linear = lin_vel;
			v.angular = ang_vel;

			// TransformPropagation runs every frame in PreStore, so children's WorldTransform
			// is updated automatically; no need to manually trigger modified().
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// === SYSTEM: Distribute collision events ===
	// Clears previous frame's events and distributes new ones to entities
	world.system("PhysicsCollisionEvents")
		.kind(flecs::OnUpdate)
		.run([](flecs::iter& it) {
			const auto world = it.world();
			const auto& sys = world.get<JoltBackend>();

			// Clear previous frame's events
			world.query<CollisionEvents>().each([](CollisionEvents& ce) { ce.events.clear(); });

			// Distribute new events
			for (const auto events = sys.GetCollisionEvents(); const auto& event : events) {
				if (auto e_a = world.entity(event.entity_a); e_a.is_valid() && e_a.has<CollisionEvents>()) {
					e_a.get_mut<CollisionEvents>().events.push_back(event);
				}
				if (auto e_b = world.entity(event.entity_b); e_b.is_valid() && e_b.has<CollisionEvents>()) {
					e_b.get_mut<CollisionEvents>().events.push_back(event);
				}
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// === SYSTEM: Debug render physics bodies ===
	// Controlled via GameData::debugPhysicsRender
#ifdef JPH_DEBUG_RENDERER
	if (debug_renderer) {
		const auto debug_render_sys = world.system("PhysicsDebugRender")
										  .kind(flecs::OnStore)
										  .run([](const flecs::iter& it) {
											  const auto& sys = it.world().get<JoltBackend>();

											  // Tell Jolt to render all bodies
											  sys.DebugDrawBodies();
										  })
										  .add<scene::GameScene>();

		// Disable initially — GameData will control it
		if (!debug_render_sys.disable()) {
			spdlog::warn("[PhysicsModule] Failed to disable debug render system");
		}

		// === SYSTEM: Sync debug render state from GameData ===
		// Captures the system handle directly instead of looking it up by string each frame
		world.system("PhysicsDebugRenderSync")
			.kind(flecs::OnUpdate)
			.run([debug_render_sys](const flecs::iter& it) {
				const auto& input_state = it.world().get<input::InputState>();
				if (input_state.keys[input::KeyCode::F3].pressed) {
					if (debug_render_sys.enabled()) {
						if (!debug_render_sys.disable()) {
							spdlog::warn("[PhysicsModule] Failed to disable debug render system");
						}
					}
					else {
						if (!debug_render_sys.enable()) {
							spdlog::warn("[PhysicsModule] Failed to enable debug render system");
						}
					}
				}
			})
			.add<scene::GameScene>();
	}
#endif

	spdlog::info("[PhysicsModule] Registered Jolt physics with Flecs");
}

} // namespace engine::physics
