#include "jolt_module.hpp"

#include <memory>

#include <flecs.h>
#include <spdlog/spdlog.h>

#include "../physics_components.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/input/input.hpp"
#include "engine/platform/platform.hpp"
#include "engine/scene/scene.hpp"
#include "engine/spatial/spatial.hpp"
#include "jolt_backend.hpp"
#include "jolt_debug_renderer.hpp"

namespace engine::physics::jolt {

namespace {
struct PhysicsAccumulator {
	float accumulated_time{0.0F};
};
} // namespace

JoltModule::JoltModule(const flecs::world& world) {
	// Initialize Jolt backend
	world.emplace<JoltBackend>(world.get<PhysicsConfig>());
	auto& physics = world.get_mut<JoltBackend>();
	if (!physics.Initialize()) {
		spdlog::error("[JoltModule] Failed to initialize Jolt");
		return;
	}

	// Initialize accumulator for fixed timestep
	world.set<PhysicsAccumulator>({});

	// === SYSTEM: Apply forces each frame ===
	// Runs once per entity that has PhysicsForce
	std::ignore =
		world.system<const PhysicsForce, JoltBackend>("PhysicsApplyForces")
			.kind(flecs::OnUpdate)
			.term_at<JoltBackend>()
			.singleton()
			.each([](const flecs::entity& e, const PhysicsForce& force, JoltBackend& sys) {
				if (sys.HasBody(e.id())) {
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
	std::ignore =
		world.system<const PhysicsImpulse, JoltBackend>("PhysicsApplyImpulses")
			.kind(flecs::OnValidate)
			.term_at<JoltBackend>()
			.singleton()
			.each([](const flecs::entity& e, const PhysicsImpulse& impulse, JoltBackend& sys) {
				if (sys.HasBody(e.id())) {
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
	std::ignore = world.system<const PhysicsVelocityOverride, JoltBackend>("PhysicsApplyVelocityOverrides")
					  .kind(flecs::OnUpdate)
					  .term_at<JoltBackend>()
					  .singleton()
					  .each([](const flecs::entity& e, const PhysicsVelocityOverride& vov, JoltBackend& sys) {
						  if (sys.HasBody(e.id())) {
							  sys.SetBodyVelocity(e.id(), vov.linear, vov.angular);
						  }
						  e.remove<PhysicsVelocityOverride>();
					  })
					  .add<ecs::Pausable>()
					  .add<scene::GameScene>();

	// === SYSTEM: Step physics simulation ===
	// Handles fixed timestep accumulation and physics stepping
	std::ignore = world.system<const JoltBackend, const PhysicsWorldConfig, PhysicsAccumulator>("PhysicsStep")
					  .kind(flecs::OnUpdate)
					  .term_at<JoltBackend>()
					  .singleton()
					  .term_at<PhysicsWorldConfig>()
					  .singleton()
					  .term_at<PhysicsAccumulator>()
					  .singleton()
					  .run([](flecs::iter& it) {
						  const auto& sys = it.world().get<JoltBackend>();
						  const auto& cfg = it.world().get<PhysicsWorldConfig>();
						  auto& acc = it.world().get_mut<PhysicsAccumulator>();

						  sys.SetGravity(cfg.gravity);

						  acc.accumulated_time += it.delta_time();

						  int steps = 0;
						  while (acc.accumulated_time >= cfg.fixed_timestep && steps < cfg.max_substeps) {
							  sys.StepSimulation(cfg.fixed_timestep);
							  acc.accumulated_time -= cfg.fixed_timestep;
							  ++steps;
						  }

						  // Clamp to prevent spiral of death
						  acc.accumulated_time =
							  std::min(acc.accumulated_time, cfg.fixed_timestep * static_cast<float>(cfg.max_substeps));

						  // Spin through the iterator to ensure ECS systems are aware of the iteration
						  while (it.next());
					  })
					  .add<ecs::Pausable>()
					  .add<scene::GameScene>();

	// === OBSERVER: Sync physics bodies to backend on component set ===
	// Triggered when RigidBody is set.
	// TODO: Come back and add a system with detect_changes() to recreate physics bodies when RigidBody or CollisionShape changes.
	// This will require a bit of care to avoid feedback loops, but it should be doable. using detect_changes() and it.changed in the loop,
	std::ignore = world.observer<const RigidBody, JoltBackend>("PhysicsAddBody")
					  .event(flecs::OnSet)
					  .term_at<JoltBackend>()
					  .singleton()
					  .run([](flecs::iter& it) {
						  auto& sys = it.world().get_mut<JoltBackend>();

						  while (it.next()) {
							  auto rb_array = it.field<const RigidBody>(0);

							  for (const size_t i : it) {
								  flecs::entity e = it.entity(i);
								  const auto& rb = rb_array[i];
								  const auto& cs = e.get<CollisionShape>();

								  const PhysicsTransform phys_transform =
									  PhysicsTransform::FromMatrix(e.get<const spatial::WorldTransform>().matrix);
								  sys.SyncBodyToBackend(e.id(), phys_transform, rb, cs);

								  if (rb.motion_type == MotionType::Dynamic && !e.has<PhysicsVelocity>()) {
									  e.set<PhysicsVelocity>({});
								  }
							  }
						  }
					  });

	// === OBSERVER: Remove bodies from backend on RigidBody removal ===
	std::ignore = world.observer<const RigidBody, JoltBackend>("PhysicsRemoveBody")
					  .event(flecs::OnRemove)
					  .term_at<JoltBackend>()
					  .singleton()
					  .each([](const flecs::entity& e, const RigidBody&, JoltBackend& sys) { sys.RemoveBody(e.id()); });

	// === SYSTEM: Sync physics state back to ECS ===
	std::ignore = world.system<const spatial::WorldTransform, const RigidBody>("PhysicsSyncToBackend")
					  .kind(flecs::OnValidate)
					  .detect_changes()
					  .run([](flecs::iter& it) {
						  auto& sys = it.world().get_mut<JoltBackend>();

						  while (it.next()) {
							  if (!it.changed()) {
								  continue;
							  }

							  const auto wt_array = it.field<const spatial::WorldTransform>(0);

							  for (const size_t i : it) {
								  flecs::entity e = it.entity(i);
								  const PhysicsTransform phys_transform =
									  PhysicsTransform::FromMatrix(wt_array[i].matrix);
								  sys.SetBodyTransform(e.id(), phys_transform);
							  }
						  }
					  });

	std::ignore =
		world.system<const spatial::WorldTransform, const PhysicsVelocity, const RigidBody>("PhysicsSyncFromBackend")
			.kind(flecs::PostUpdate)
			.run([](flecs::iter& it) {
				auto& sys = it.world().get<JoltBackend>();

				while (it.next()) {
					auto wt_array = it.field<const spatial::WorldTransform>(0);
					auto vel_array = it.field<const PhysicsVelocity>(1);

					for (const size_t i : it) {
						auto [pos, rot, lin_vel, ang_vel] = sys.GetPhysicsSyncResult(it.entity(i).id());
						// We must use const_cast to modify the WorldTransform so it won't trigger a modified event and cause a feedback loop.
						// The modified event is triggered by the PhysicsSyncToBackend system, which will update the physics body in the backend.
						auto& wt = const_cast<spatial::WorldTransform&>(wt_array[i]);
						wt.position = pos;
						wt.rotation = glm::eulerAngles(rot);
						// Preserve scale from TransformPropagation
						wt.ComputeMatrix();

						auto& vel = const_cast<PhysicsVelocity&>(vel_array[i]);
						vel.linear = lin_vel;
						vel.angular = ang_vel;
					}
				}
			})
			.add<ecs::Pausable>()
			.add<scene::GameScene>();

	// === SYSTEM: Distribute collision events ===
	// Clears previous frame's events and distributes new ones to entities
	std::ignore =
		world.system("PhysicsCollisionEvents")
			.kind(flecs::PostFrame)
			.run([](const flecs::iter& it) {
				const auto ecs_world = it.world();
				const auto& sys = ecs_world.get<JoltBackend>();

				// Clear previous frame's events
				ecs_world.query<CollisionEvents>().each([](CollisionEvents& ce) { ce.events.clear(); });

				// Distribute new events
				for (const auto events = sys.GetCollisionEvents(); const auto& event : events) {
					if (auto e_a = ecs_world.entity(event.entity_a); e_a.is_valid() && e_a.has<CollisionEvents>()) {
						e_a.get_mut<CollisionEvents>().events.push_back(event);
					}
					if (auto e_b = ecs_world.entity(event.entity_b); e_b.is_valid() && e_b.has<CollisionEvents>()) {
						e_b.get_mut<CollisionEvents>().events.push_back(event);
					}
				}
			})
			.add<ecs::Pausable>()
			.add<scene::GameScene>();

	// === SYSTEM: Debug render physics bodies ===
	// Controlled via GameData::debugPhysicsRender
#ifdef JPH_DEBUG_RENDERER
	auto& platform = world.get_mut<platform::PlatformRef>();
	const auto debug_renderer = std::make_shared<JoltDebugRendererAdapter>(world, platform.ptr);
	physics.SetDebugRenderer(debug_renderer.get());
	world.set<std::shared_ptr<JoltDebugRendererAdapter>>(debug_renderer);

	const auto debug_render_sys = world.system< const JoltBackend>("PhysicsDebugRender")
									  .kind(flecs::OnStore)
									  .each([](const JoltBackend& sys) { sys.DebugDrawBodies(); })
									  .add<scene::GameScene>();

	// Disable initially — GameData will control it
	if (!debug_render_sys.disable()) {
		spdlog::warn("[JoltModule] Failed to disable debug render system");
	}

	// === SYSTEM: Sync debug render state from GameData ===
	// Captures the system handle directly instead of looking it up by string each frame
	std::ignore = world.system<const input::InputState>("PhysicsDebugRenderSync")
					  .kind(flecs::OnUpdate)
					  .term_at<input::InputState>()
					  .singleton()
					  .each([debug_render_sys](const input::InputState& input_state) {
						  if (input_state.keys[input::KeyCode::F3].pressed) {
							  if (debug_render_sys.enabled()) {
								  if (!debug_render_sys.disable()) {
									  spdlog::warn("[JoltModule] Failed to disable debug render system");
								  }
							  }
							  else {
								  if (!debug_render_sys.enable()) {
									  spdlog::warn("[JoltModule] Failed to enable debug render system");
								  }
							  }
						  }
					  })
					  .add<scene::GameScene>();

#endif

	spdlog::info("[JoltModule] Initialized Jolt physics backend");
}

} // namespace engine::physics::jolt
