# Physics System

Concise Jolt Physics integration with Flecs ECS using standard C++ headers/sources.

## Structure

- **physics_types.hpp** — Core physics types, enums (MotionType, ShapeType, etc.), and data structures
- **gltf_geometry.hpp/cpp** — CPU glTF/GLB triangle loader for mesh colliders (cgltf, no raylib)
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

A rigid body's shape sits at `CollisionShape.offset` (and `rotation`, C++ only) from the entity origin, for every shape
type; the body's position stays the entity's. A character's capsule uses `offset` too, to put its origin at the feet
(see below).

### Mesh colliders

A `mesh` shape makes a loaded model solid. It collides against every triangle of every mesh in the file, with each
node's transform applied (so a multi-node `.glb` comes out assembled):

```json
"rigid_body": { "motion_type": "static" },
"collision_shape": { "type": "mesh", "mesh_path": "models/room.glb" }
```

- `mesh_path` is read only when `type` is `mesh`. Level JSON resolves it against the asset directory with
  `assets::ResolveAsset` (`engine/assets/assets.hpp`, the same call `MeshPrimitive` uses), so a level can use the same
  string for `mesh` and `collision_shape`. C++ and script authors pass a path in the form `ResolveAsset` produces; the
  collider does not resolve it again.
- **Static bodies only.** A mesh on a dynamic or kinematic body logs an error and creates no body.
- A missing or unreadable file, or one with zero triangles, logs one error naming the entity and path and creates no
  body. A `convex_hull` shape has no vertex source yet, so it logs instead of becoming a box.
- Entities with the same `mesh_path` share one built shape (cached until the backend shuts down).
- `CollisionShape.offset` and `rotation` are honored. `Transform.scale` on a mesh is **unsupported**.
- The shape keeps the file's winding. A `CharacterVirtual` collides with back faces, but rigid bodies collide
  one-sided, so check that a converted asset faces into the room.
- The CPU loader (`gltf_geometry.hpp`, built on cgltf) has no raylib dependency, so a headless server can build the
  same collision world.



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

## Character body

A walking character is a capsule `collision_shape` plus `character_body`, backed by Jolt's `CharacterVirtual`. It is
not a rigid body: pairing it with a `rigid_body`, or with a non-capsule shape, logs an error and creates no character.

```json
{"name": "Player", "components": {
  "character_body": {"max_slope_deg": 50, "step_height": 0.4, "stick_to_floor": 0.5, "mass": 70},
  "collision_shape": {"type": "capsule", "capsule_radius": 0.4, "capsule_height": 1.0, "offset": [0, 0.9, 0]},
  "gravity": {"acceleration": [0, -9.81, 0]},
  "transform": {"position": [0, 1, 0]}
}}
```

| Component | Role |
|---|---|
| `CharacterBody` | Authored: max slope (degrees), step height, stick-to-floor distance, mass, layer. Defaults are Jolt's. `layer` is stored only; the backend has just moving and non-moving object layers. |
| `CharacterMove` | Runtime input. The game writes `velocity` each frame; only the part perpendicular to up is used. Absent means zero. |
| `Gravity` | Authored. The one acceleration an entity feels; up is `-normalize(acceleration)`. Absent means `PhysicsWorldConfig.gravity`. Only characters read it. |
| `CharacterState` | Runtime, added with `CharacterBody`: `grounded`, `ground_entity`, `ground_normal`, `velocity`. |

`CollisionShape.offset` is the capsule center relative to the entity origin (put the origin at the feet by offsetting
half the height plus the radius), expressed in the character's up-aligned frame.

**Step order**, per substep inside `PhysicsStep`, before `PhysicsSystem::Update`:

1. Project `CharacterMove.velocity` onto the plane perpendicular to up.
2. Vertical speed resets to 0 when grounded and not moving away from the ground; gravity adds `gravity * dt`.
3. `SetLinearVelocity`, then `ExtendedUpdate` (stick to floor, walk stairs).

After the step, `CharacterSyncFromBackend` (PostUpdate) writes the position to `Transform.position` and fills
`CharacterState`. It never writes `Transform.rotation`.

**Teleport:** `CharacterTeleport` compares `Transform.position` to the position last written back; a difference moves
the character there. Rotation is ignored, so a game can drive facing every frame.

Out of scope: jump, pushing dynamic bodies, an inner body (raycasts don't hit a character), and saving or restoring
`CharacterVirtual` state.

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
