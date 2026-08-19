# Physics System

Concise Jolt Physics integration with Flecs ECS using standard C++ headers/sources.

## Structure

- **physics_types.hpp** — Core physics types, enums (MotionType, ShapeType, etc.), and data structures
- **physics_components.hpp** — ECS components (RigidBody, CollisionShape, PhysicsVelocity, etc.)
- **jolt_backend.hpp/cpp** — Concrete JoltPhysicsSystem implementation (no interfaces)
- **physics_module.hpp/cpp** — Flecs system registration and ECS integration
- **physics.hpp** — Main public API header

## Usage

```cpp
// Create physics module (registers all Flecs systems)
engine::physics::PhysicsModule physics_module(world);

// Physics system automatically registers:
// - PhysicsApplyForces — applies PhysicsForce components each frame
// - PhysicsApplyImpulses — applies PhysicsImpulse components and removes them
// - PhysicsStep — fixed timestep physics stepping with accumulation
// - PhysicsSyncToBackend — syncs ECS state to Jolt on component changes
// - PhysicsRemoveBody — removes bodies when RigidBody removed
// - PhysicsSyncFromBackend — syncs dynamic bodies back from Jolt
// - PhysicsCollisionEvents — distributes collision events to entities
```

## Adding Physics to an Entity

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

1. **Per-entity systems** (`.each()`):

- PhysicsApplyForces
- PhysicsApplyImpulses
- PhysicsSyncFromBackend

2. **Global systems** (`.run()` with singleton access):

- PhysicsStep
- PhysicsCollisionEvents

3. **Observers** (event-based):

- PhysicsSyncToBackend (OnSet)
- PhysicsRemoveBody (OnRemove)

Systems run in `OnUpdate` (logic) or `OnStore` (debug rendering), scoped to `scene::GameScene` via
`.add<scene::GameScene>()` on the system entity, with proper ordering for the physics simulation pipeline.
