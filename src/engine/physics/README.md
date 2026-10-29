# Physics System

Concise Jolt Physics integration with Flecs ECS using standard C++ headers/sources.

## Structure

- **physics_types.hpp** — Core physics types, enums (MotionType, ShapeType, etc.), and data structures
- **physics_components.hpp** — ECS components (RigidBody, CollisionShape, PhysicsVelocity, etc.)
- **physics_module.hpp/cpp** — Flecs system registration, ECS integration, and physics-world setup
- **jolt/jolt_backend.hpp/cpp** — Low-level Jolt API wrapper (physics shape/body creation, stepping)
- **jolt/jolt_module.hpp/cpp** — Jolt-specific Flecs systems (sync, step, collision events)
- **jolt/jolt_debug_renderer.cpp** — Debug visualization support
- **physics.hpp** — Main public API header

## Usage

```cpp
// Create physics module (registers physics-world setup and observers)
engine::physics::PhysicsModule physics_module(world);

// Additionally creates Jolt physics systems (lives in jolt_module.cpp):
// - PhysicsApplyForces — applies PhysicsForce components each frame
// - PhysicsApplyImpulses — applies PhysicsImpulse components and removes them
// - PhysicsStep — fixed timestep physics stepping with accumulation
// - PhysicsSyncToBackend — syncs ECS state to Jolt on component changes (OnSet observer)
// - PhysicsRemoveBody — removes bodies when RigidBody removed (OnRemove observer)
// - PhysicsSyncFromBackend — syncs dynamic bodies back from Jolt
// - PhysicsCollisionEvents — distributes collision events to entities
```

## RigidBody + CollisionShape — Required Pair

`RigidBody` and `CollisionShape` are a **required pair** — a physics body cannot be created without both:

- `CollisionShape` **must** be present when `RigidBody` is added, **or** added immediately after in the same
  deferred command batch.
- If `RigidBody` is added without a `CollisionShape`, a warning is logged and body creation is **deferred** until
  `CollisionShape` is set (handled by `PhysicsAddBodyOnCollisionShape`).
- `CollisionShape` *can* exist without `RigidBody` (e.g. trigger volumes or shapes awaiting attachment).

In JSON level files, always author both components on the same entity:

```json
"rigid_body": { "motion_type": "static" },
"collision_shape": { "type": "box", "box_half_extents": [0.5, 0.5, 0.5] }
```



```cpp
auto entity = world.entity();
entity.set<engine::components::Transform>({{0, 1, 0}});
entity.set<engine::components::WorldTransform>({{0, 1, 0}, glm::quat(1,0,0,0), {1,1,1}, glm::mat4(1)});

entity.set<engine::physics::RigidBody>({
    .motion_type = engine::physics::MotionType::Dynamic,
    .mass = 1.0F,
    .friction = 0.5F,
    .restitution = 0.3F
});

entity.set<engine::physics::CollisionShape>({
    .type = engine::physics::ShapeType::Box,
    .box_half_extents = {0.5F, 0.5F, 0.5F}
});
```

## Flecs System Registration Patterns

All systems follow Flecs 4.x best practices:

### Physics-world setup (physics_module.cpp)
- World-level observer for seeding physics metadata, state tracking

### Jolt-specific systems (jolt_module.cpp)

1. **Per-entity systems** (`.each()`):

- PhysicsApplyForces
- PhysicsApplyImpulses
- PhysicsSyncFromBackend

2. **Global systems** (`.run()` with singleton access):

- PhysicsStep
- PhysicsCollisionEvents

3. **Observers** (event-based):

- PhysicsAddBody (OnSet RigidBody — creates body if CollisionShape already present)
- PhysicsAddBodyOnCollisionShape (OnSet CollisionShape + has RigidBody — creates body if not yet created)
- PhysicsSetBodyTransform (OnSet WorldTransform + has RigidBody — moves the body to the seeded transform; setting a
  rigid body's `Transform` teleports it)
- PhysicsSyncToBackend (OnSet — syncs ECS transform changes to Jolt)
- PhysicsRemoveBody (OnRemove)

Systems run in `OnUpdate` (logic) or `OnStore` (debug rendering), scoped to `scene::GameScene` via
`.add<scene::GameScene>()` on the system entity, with proper ordering for the physics simulation pipeline.
