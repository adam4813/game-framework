#include "jolt_module.hpp"

#include <memory>
#include <string>

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
// Name for log messages: the entity's name, or its id when unnamed.
std::string EntityLabel(const flecs::entity& e) { return e.name() ? std::string{e.name()} : std::to_string(e.id()); }

// Creates the entity's character once it has both CharacterBody and a CollisionShape. Called from the observers
// for either component, since level JSON applies components alphabetically.
void CreateCharacter(JoltBackend& sys, const flecs::entity e) {
	if (sys.HasCharacter(e.id())) return;
	const auto* body = e.try_get<CharacterBody>();
	const auto* shape = e.try_get<CollisionShape>();
	if (!body || !shape) return;
	if (e.has<RigidBody>()) {
		spdlog::error("[Physics] Entity '{}': a character_body can't also be a rigid_body.", EntityLabel(e));
		return;
	}
	const auto* local = e.try_get<spatial::Transform>();
	sys.AddCharacter(e.id(), EntityLabel(e), local ? local->position : glm::vec3{0.0F}, *body, *shape);
}

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER
// Creates the debug renderer that draws bodies through `platform`, and the PhysicsDebugRender system (off until the
// debug render key toggles it). Constructing the adapter makes it Jolt's global DebugRenderer::sInstance.
void InstallDebugRenderer(const flecs::world& world, JoltBackend& physics, platform::Platform* platform) {
	const auto debug_renderer = std::make_shared<JoltDebugRendererAdapter>(world, platform);
	physics.SetDebugRenderer(debug_renderer.get());
	world.set<std::shared_ptr<JoltDebugRendererAdapter>>(debug_renderer);

	const auto debug_render_sys = world.system< const JoltBackend>("PhysicsDebugRender")
									  .kind(flecs::OnStore)
									  .each([](const JoltBackend& sys) { sys.DebugDrawBodies(); })
									  .add<scene::GameScene>();

	// Off until the debug render key turns it on.
	if (!debug_render_sys.disable()) {
		spdlog::warn("[JoltModule] Failed to disable debug render system");
	}

	// === SYSTEM: Toggle debug rendering on the debug render key ===
	// Captures the system handle directly instead of looking it up by string each frame
	std::ignore = world.system<const input::InputState>("PhysicsDebugRenderSync")
					  .kind(flecs::OnUpdate)
					  .term_at<input::InputState>()
					  .singleton()
					  .each([debug_render_sys](const input::InputState& input_state) {
						  if (input_state.keys[input::kDebugRenderKey].pressed) {
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
}
#endif

} // namespace

JoltModule::JoltModule(const flecs::world& world) {
	// Initialize Jolt backend
	world.emplace<JoltBackend>(world.get<PhysicsConfig>());
	auto& physics = world.get_mut<JoltBackend>();
	if (!physics.Initialize()) {
		spdlog::error("[JoltModule] Failed to initialize Jolt");
		return;
	}

	// The simulation systems below are FixedStep: each runs once per Tick, in phase order, with the fixed dt (see
	// engine/fixed_step/README.md). Pause stops the whole fixed step, so they don't carry Pausable.

	// === SYSTEM: Apply forces each Tick ===
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
			.add<ecs::FixedStep>()
			.add<scene::GameScene>();

	// === SYSTEM: Apply impulses each Tick ===
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
			.add<ecs::FixedStep>()
			.add<scene::GameScene>();

	// === SYSTEM: Apply velocity overrides each Tick ===
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
					  .add<ecs::FixedStep>()
					  .add<scene::GameScene>();

	// === Characters ===
	// A Transform position that differs from the one last written back is a teleport; rotation is ignored, so a game
	// can drive facing every frame. Runs before PhysicsStep so the move starts from the teleported spot.
	std::ignore = world.system<const spatial::Transform>("CharacterTeleport")
					  .with<CharacterBody>()
					  .kind(flecs::OnUpdate)
					  .each([](const flecs::iter& it, const size_t i, const spatial::Transform& local) {
						  auto& sys = it.world().get_mut<JoltBackend>();
						  const flecs::entity_t id = it.entity(i).id();
						  if (sys.HasCharacter(id) && local.position != sys.GetCharacterWrittenPosition(id)) {
							  sys.SetCharacterPosition(id, local.position);
						  }
					  })
					  .add<ecs::FixedStep>()
					  .add<scene::GameScene>();

	// Reacts to OnSet (not OnAdd) so the observer reads the authored values, and tolerates either component arriving
	// first.
	std::ignore = world.observer<const CharacterBody, JoltBackend>("CharacterAdd")
					  .event(flecs::OnSet)
					  .term_at<JoltBackend>()
					  .singleton()
					  .run([](flecs::iter& it) {
						  auto& sys = it.world().get_mut<JoltBackend>();
						  while (it.next()) {
							  for (const size_t i : it) {
								  CreateCharacter(sys, it.entity(i));
							  }
						  }
					  });
	std::ignore = world.observer<const CollisionShape, JoltBackend>("CharacterAddOnCollisionShape")
					  .event(flecs::OnSet)
					  .with<CharacterBody>()
					  .term_at<JoltBackend>()
					  .singleton()
					  .run([](flecs::iter& it) {
						  auto& sys = it.world().get_mut<JoltBackend>();
						  while (it.next()) {
							  for (const size_t i : it) {
								  CreateCharacter(sys, it.entity(i));
							  }
						  }
					  });
	// A rigid_body arriving after the character_body (JSON order) makes the pairing invalid.
	std::ignore =
		world.observer<const RigidBody, JoltBackend>("CharacterRejectRigidBody")
			.event(flecs::OnSet)
			.with<CharacterBody>()
			.term_at<JoltBackend>()
			.singleton()
			.each([](const flecs::entity e, const RigidBody&, JoltBackend& sys) {
				if (!sys.HasCharacter(e.id())) return;
				spdlog::error("[Physics] Entity '{}': a character_body can't also be a rigid_body.", EntityLabel(e));
				sys.RemoveCharacter(e.id());
			});
	std::ignore =
		world.observer<const CharacterBody, JoltBackend>("CharacterRemove")
			.event(flecs::OnRemove)
			.term_at<JoltBackend>()
			.singleton()
			.each([](const flecs::entity& e, const CharacterBody&, JoltBackend& sys) { sys.RemoveCharacter(e.id()); });
	std::ignore =
		world.observer<const CollisionShape, JoltBackend>("CharacterRemoveOnCollisionShape")
			.event(flecs::OnRemove)
			.with<CharacterBody>()
			.term_at<JoltBackend>()
			.singleton()
			.each([](const flecs::entity& e, const CollisionShape&, JoltBackend& sys) { sys.RemoveCharacter(e.id()); });

	auto characters = world.query_builder<const CharacterMove*, const Gravity*>().with<CharacterBody>().build();

	// === SYSTEM: Step physics simulation ===
	// Steps the characters and then the world exactly once per Tick, by the fixed dt.
	std::ignore =
		world.system<const JoltBackend, const PhysicsWorldConfig>("PhysicsStep")
			.kind(flecs::OnUpdate)
			.term_at<JoltBackend>()
			.singleton()
			.term_at<PhysicsWorldConfig>()
			.singleton()
			.run([characters](flecs::iter& it) {
				auto& sys = it.world().get_mut<JoltBackend>();
				const auto& cfg = it.world().get<PhysicsWorldConfig>();
				const float dt = it.delta_time();

				sys.SetGravity(cfg.gravity);

				// Characters move first so the simulation step sees their new positions.
				characters.each([&](const flecs::entity e, const CharacterMove* move, const Gravity* gravity) {
					sys.StepCharacter(
						e.id(),
						dt,
						{.move_velocity = move ? move->velocity : glm::vec3{0.0F},
						 .gravity = gravity ? gravity->acceleration : cfg.gravity}
					);
				});
				sys.StepSimulation(dt);

				// Spin through the iterator to ensure ECS systems are aware of the iteration
				while (it.next());
			})
			.add<ecs::FixedStep>()
			.add<scene::GameScene>();

	// === OBSERVER: Create physics body when RigidBody is set ===
	// Reacts to OnSet, not OnAdd: inside a defer_suspend()ed region (a scene's own level load) `set<RigidBody>` adds the component
	// default-constructed and fires OnAdd before copying the value in, so an OnAdd observer would see
	// MotionType::Dynamic and build the wrong body. HasBody makes repeated sets a no-op.
	// CollisionShape MUST already be present — RigidBody without CollisionShape is an authoring
	// error and will NOT create a body. If CollisionShape arrives after RigidBody (e.g. component
	// order in a level file), PhysicsAddBodyOnCollisionShape below handles that case.
	// TODO: Add system with detect_changes() to recreate bodies when RigidBody/CollisionShape changes.
	std::ignore =
		world.observer<const RigidBody, JoltBackend>("PhysicsAddBody")
			.event(flecs::OnSet)
			.term_at<JoltBackend>()
			.singleton()
			.run([](flecs::iter& it) {
				auto& sys = it.world().get_mut<JoltBackend>();

				while (it.next()) {
					auto rb_array = it.field<const RigidBody>(0);

					for (const size_t i : it) {
						flecs::entity e = it.entity(i);
						if (sys.HasBody(e.id())) {
							continue;
						}
						const auto& rb = rb_array[i];
						const auto* cs = e.try_get<CollisionShape>();

						if (!cs) {
							const std::string entity_name = EntityLabel(e);
							spdlog::warn(
								"[Physics] Entity '{}' has RigidBody but no CollisionShape — body creation deferred "
								"until CollisionShape is added.",
								entity_name
							);
							continue;
						}

						const auto* wt = e.try_get<const spatial::WorldTransform>();
						const PhysicsTransform phys_transform =
							wt ? PhysicsTransform::FromMatrix(wt->matrix)
							   : PhysicsTransform{}; // identity; corrected by PhysicsSetBodyTransform

						sys.SyncBodyToBackend(e.id(), EntityLabel(e), phys_transform, rb, *cs);

						if (rb.motion_type == MotionType::Dynamic && !e.has<PhysicsVelocity>()) {
							e.set<PhysicsVelocity>({});
						}
					}
				}
			});

	// === OBSERVER: Create physics body when CollisionShape is added to an entity with RigidBody ===
	// Handles the case where CollisionShape is authored after RigidBody (e.g. JSON component order).
	// Mirrors the PhysicsAddBody logic; also handles CollisionShape being set/updated after creation.
	std::ignore = world.observer<const CollisionShape, JoltBackend>("PhysicsAddBodyOnCollisionShape")
					  .event(flecs::OnSet)
					  .with<RigidBody>()
					  .term_at<JoltBackend>()
					  .singleton()
					  .run([](flecs::iter& it) {
						  auto& sys = it.world().get_mut<JoltBackend>();

						  while (it.next()) {
							  auto cs_array = it.field<const CollisionShape>(0);

							  for (const size_t i : it) {
								  const flecs::entity e = it.entity(i);

								  // Skip if a body was already created by PhysicsAddBody (CS was present at RB add time).
								  if (sys.HasBody(e.id())) {
									  continue;
								  }

								  const auto& cs = cs_array[i];
								  const auto& rb = e.get<const RigidBody>();

								  const auto* wt = e.try_get<const spatial::WorldTransform>();
								  const PhysicsTransform phys_transform =
									  wt ? PhysicsTransform::FromMatrix(wt->matrix) : PhysicsTransform{};

								  sys.SyncBodyToBackend(e.id(), EntityLabel(e), phys_transform, rb, cs);

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

	// Moves the body whenever its WorldTransform is set: the Transform seed (TransformSeedWorldTransform)
	// after a level load or spawn, or a teleport. Covers a body created before its transform arrived
	// (JSON component order puts rigid_body before transform), which starts at the identity. OnSet,
	// not OnAdd: an undeferred set fires OnAdd with the default value, which would pin it to the origin.
	std::ignore = world.observer<const spatial::WorldTransform, JoltBackend>("PhysicsSetBodyTransform")
					  .event(flecs::OnSet)
					  .with<RigidBody>()
					  .term_at<JoltBackend>()
					  .singleton()
					  .each([](const flecs::entity e, const spatial::WorldTransform& wt, JoltBackend& sys) {
						  if (sys.HasBody(e.id())) {
							  sys.SetBodyTransform(e.id(), PhysicsTransform::FromMatrix(wt.matrix));
						  }
					  });

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
			.add<ecs::FixedStep>()
			.add<scene::GameScene>();

	// Writes the character's position to its Transform (never the rotation, which the game owns) and fills
	// CharacterState. In place, without modified<>: TransformPropagation recomputes WorldTransform every frame.
	std::ignore =
		world.system<spatial::Transform, CharacterState>("CharacterSyncFromBackend")
			.with<CharacterBody>()
			.kind(flecs::PostUpdate)
			.each([](const flecs::iter& it, const size_t i, spatial::Transform& local, CharacterState& state) {
				auto& sys = it.world().get_mut<JoltBackend>();
				const flecs::entity_t id = it.entity(i).id();
				if (!sys.HasCharacter(id)) return;
				const CharacterSyncResult result = sys.GetCharacterSyncResult(id);
				local.position = result.position;
				sys.SetCharacterWrittenPosition(id, result.position);
				state = {
					.grounded = result.grounded,
					.ground_entity = result.ground_entity,
					.ground_normal = result.ground_normal,
					.velocity = result.velocity
				};
			})
			.add<ecs::FixedStep>()
			.add<scene::GameScene>();

	// === SYSTEM: Distribute collision events ===
	// Per Frame, not per Tick: clears the previous Frame's events and distributes the ones this Frame's Ticks produced
	std::ignore =
		world.system("PhysicsCollisionEvents")
			.kind(flecs::PostFrame)
			.run([](const flecs::iter& it) {
				const auto ecs_world = it.world();
				auto& sys = ecs_world.get_mut<JoltBackend>();

				// Clear previous frame's events
				ecs_world.query<CollisionEvents>().each([](CollisionEvents& ce) { ce.events.clear(); });

				// Distribute the events of every Tick since the last Frame
				for (const auto events = sys.TakeCollisionEvents(); const auto& event : events) {
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

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER
	// Jolt's debug renderer is one process-global (DebugRenderer::sInstance), so only a world that draws installs it.
	// A headless world (a server beside a client) never creates one, so it never clears the one another world set.
	if (auto* host = world.get<platform::PlatformRef>().ptr; host->HasWindow()) {
		InstallDebugRenderer(world, physics, host);
	}
#endif

	spdlog::info("[JoltModule] Initialized Jolt physics backend");
}

} // namespace engine::physics::jolt
