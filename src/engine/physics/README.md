# Physics System

Concise Jolt Physics integration with Flecs ECS using standard C++ headers/sources.

## Structure

- **physics_types.hpp** — Core physics types, enums (MotionType, ShapeType, etc.), and data structures
- **gltf_geometry.hpp/cpp** — CPU glTF/GLB triangle loader for mesh colliders (cgltf, no raylib); it compiles the
  engine's prefixed cgltf copy (`engine/platform/cgltf_prefixed.hpp`)
- **physics_components.hpp** — ECS components (RigidBody, CollisionShape, PhysicsVelocity, etc.)
- **physics_module.hpp/cpp** — Flecs system registration, ECS integration, and physics-world setup
- **physics_queries.hpp/cpp** — `Raycast` and `RaycastAll` (see Queries); no Jolt types in the API
- **physics_script_bindings.hpp/cpp** — the queries' AngelScript bindings and `Layer_*` constants
- **jolt/jolt_backend.hpp/cpp** — Low-level Jolt API wrapper (physics shape/body creation, stepping, queries)
- **jolt/jolt_module.hpp/cpp** — Jolt-specific Flecs systems (sync, step, collision events)
- **jolt/character_step.hpp/cpp** — `RestoreCharacter` and `StepCharacterTick` (put one character back to a given
  state, then step it one Tick at a time, for replay) and the input and write-back helpers they share with
  `PhysicsStep` and `CharacterSyncFromBackend`
- **jolt/jolt_debug_renderer.cpp** — Debug visualization support
- **physics.hpp** — Main public API header

## Usage

```cpp
// Create physics module (registers physics-world setup and observers)
engine::physics::PhysicsModule physics_module(world);

// Additionally creates Jolt physics systems (lives in jolt_module.cpp). The simulation ones are FixedStep: each runs
// once per fixed Tick with the fixed dt (engine/fixed_step/README.md), so drive the world with fixed_step::RunFrame.
// - PhysicsApplyForces — applies PhysicsForce components each Tick
// - PhysicsApplyImpulses — applies PhysicsImpulse components and removes them
// - PhysicsStep — steps the characters and the world once per Tick
// - PhysicsSyncToBackend — syncs ECS state to Jolt on component changes (OnSet observer)
// - PhysicsRemoveBody — removes bodies when RigidBody removed (OnRemove observer)
// - PhysicsSyncFromBackend — syncs dynamic bodies back from Jolt
// - PhysicsCollisionEvents — each Frame, distributes the collision events of that Frame's Ticks to entities
```

## RigidBody + CollisionShape — Required Pair

`RigidBody` and `CollisionShape` are a **required pair** — a physics body cannot be created without both:

- `CollisionShape` **must** be present when `RigidBody` is added, **or** added immediately after in the same
  deferred command batch.
- If `RigidBody` is added without a `CollisionShape`, a warning is logged and body creation is **deferred** until
  `CollisionShape` is set (handled by `PhysicsSyncBodyOnCollisionShape`).
- `CollisionShape` *can* exist without `RigidBody` (e.g. trigger volumes or shapes awaiting attachment).

In JSON level files, always author both components on the same entity:

```json
"rigid_body": { "motion_type": "static" },
"collision_shape": { "type": "box", "box_half_extents": [0.5, 0.5, 0.5] }
```

A rigid body's shape sits at `CollisionShape.offset` (and `rotation`, C++ only) from the entity origin, for every shape
type; the body's position stays the entity's. A character's capsule uses `offset` too, to put its origin at the feet
(see below).

### Changing a body after creation

Setting `RigidBody` or `CollisionShape` again (from C++, a script or the REST API) reaches the existing body:

- **Same values as last applied:** nothing happens. Repeated sets during a scene load are free.
- **In place** (the body keeps its identity, position and velocity): `motion_type` among dynamic, kinematic and
  static (a body created kinematic or dynamic can go static and back), `layer`, `friction`, `restitution`,
  `linear_damping`, `angular_damping`, `use_gravity` / `gravity_scale`, `enable_ccd` and `mass` (inertia is
  recomputed from the shape, as at creation). `collision_mask` is stored only, so it changes nothing yet.
- **Recreated** at its current position, keeping its linear and angular velocity: any `CollisionShape` change, and a
  body created static becoming kinematic or dynamic (Jolt creates static bodies without the motion data that needs).
- If the recreated body's shape can't be built (e.g. a mesh collider on a body set to dynamic), the error is logged
  once and the old body keeps its shape, and stays static if it was created static, but takes the rest of the change.
  Setting the same values again doesn't retry; a different shape does.
- A dynamic body gets `PhysicsVelocity` if it lacks one. A body set to static stops where it is.
- Moving a body is a `Transform` set (a teleport), not a `RigidBody` set.

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
  "character_body": {"max_slope_deg": 50, "step_height": 0.4, "stick_to_floor": 0.5, "mass": 70,
                     "layer": "player", "inner_body": true},
  "collision_shape": {"type": "capsule", "capsule_radius": 0.4, "capsule_height": 1.0, "offset": [0, 0.9, 0]},
  "gravity": {"acceleration": [0, -9.81, 0]},
  "transform": {"position": [0, 1, 0]}
}}
```

| Component | Role |
|---|---|
| `CharacterBody` | Authored: max slope (degrees), step height, stick-to-floor distance, mass (defaults are Jolt's), `layer` (default `player`) and `inner_body` (default `true`). |
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

**Replay** (re-simulating a predicted character after a correction): put it back to the Server's state with
`RestoreCharacter`, then step it alone, one Tick per call, with `StepCharacterTick` (both in `jolt/character_step.hpp`,
exported by `engine/engine.hpp`).

- `RestoreCharacter(character, position, velocity)` moves the character, re-detects its ground contacts, and writes the
  position to `Transform` and the ground state and velocity to `CharacterState`. Writing `Transform` is what keeps the
  next Tick's `CharacterTeleport` from moving it back to where it was. The part of the velocity along up survives the
  next step; `CharacterMove` replaces the planar part.
- `StepCharacterTick(character)` runs the same movement update and write-back as `PhysicsStep` and
  `CharacterSyncFromBackend` (they share `MakeCharacterStepInput` and `WriteCharacterBack`), so a replayed character
  lands where a Tick would put it. It steps no other character or body and doesn't advance the Tick counter.

```cpp
engine::physics::RestoreCharacter(player, server.position, server.velocity);
for (const auto& input : inputs_since_server_tick) {
    player.set<engine::physics::CharacterMove>({.velocity = input.move});
    engine::physics::StepCharacterTick(player);
}
```

Call the pair outside `progress()`, or from an immediate system in the Tick before `PhysicsStep` (e.g. a FixedStep
system with `.kind(flecs::OnLoad).immediate()`); that Tick's `PhysicsStep` then steps the character once more from where
the replay left it. Inside the system, wrap the replay in `it.world().defer_suspend()` / `defer_resume()`: an immediate
system's commands are still deferred, so without it each `set<CharacterMove>` waits in the command queue and the steps
replay the old input. Never call them from a `multi_threaded` system: they share the physics step's temp allocator,
which is only safe while no other thread is in the backend.

**Inner body:** with `inner_body` (the default), the character also gets a kinematic body in the physics system: the
same capsule and offset as its `CollisionShape`, on `CharacterBody.layer`, carrying the entity as user data. It gives
the character presence:

- Raycasts hit it (see Queries) and report the character's entity.
- Dynamic bodies bounce off it, and characters block each other (each character collides with the others' inner
  bodies; Jolt keeps a character from colliding with its own).
- It follows every move: each step, a teleport (`CharacterTeleport` / `SetCharacterPosition`) and a replayed
  `StepCharacterTick`.
- Contacts with it reach `CollisionEvents` like any body's.

Set `"inner_body": false` to let rays and other characters pass through a character. `CharacterBody` is read once,
when the character is created (it has both `CharacterBody` and its `CollisionShape`), so changing `layer` or
`inner_body` afterwards does nothing.

Out of scope: jump, a character pushing or being pushed by dynamic bodies, and saving or restoring `CharacterVirtual`
state beyond position and velocity.

## Collision layers

`RigidBody.layer` and `CharacterBody.layer` put a body on a `CollisionLayer`: `default`, `player`, `enemy`, `obstacle`
or `trigger` in level JSON (`"layer": "enemy"`). A rigid body defaults to `default`, a character to `player`.

The backend encodes each Jolt object layer as (layer, moving bit), giving 10 object layers on the 2 broad-phase layers
(non-moving and moving). **Contacts look only at the moving bit**, as before: layers don't change which bodies collide.
Queries filter on the layer (`RaycastFilter::layer_mask`). `RigidBody.collision_mask` is stored only; nothing filters
contacts on it yet.

## Queries

`physics_queries.hpp` (included by `physics.hpp`) casts rays against the bodies and character inner bodies of a world:

```cpp
using namespace engine::physics;
const Ray aim{.origin = eye, .direction = forward, .max_distance = 50.0F};

if (const auto hit = Raycast(world, aim, {.ignore = std::span{&self, 1}})) {
    // hit->entity, hit->hit_point, hit->normal, hit->distance
}
for (const RaycastResult& hit : RaycastAll(world, aim, {.layer_mask = LayerBit(CollisionLayer::Enemy)})) { ... }
```

- **`Raycast`** returns the closest hit, or `std::nullopt`. **`RaycastAll`** returns every hit, sorted by distance, one
  per body (the nearest, for a mesh or compound the ray crosses more than once).
- **`RaycastResult`**: `entity`, `hit_point`, the surface `normal` there, and `distance` along the ray.
- **The ray:** `direction` is normalized for you. A zero-length direction or `max_distance <= 0` hits nothing. A ray
  starting inside a convex shape hits it at distance 0 (Jolt's default), with the normal facing back along the ray.
- **`RaycastFilter`**: `layer_mask` (`LayerBit`s, default every layer but `Trigger`, i.e. `kDefaultRaycastMask`) and
  `ignore`, a span of entities to pass through. Nothing is ignored implicitly, not even the caller.
- **Timing:** a query sees the bodies as of the last finished Tick, with no interpolation (a body teleported since,
  by setting its `Transform`, is seen where it was moved). Call it from any system or from outside `progress()`; a
  query made while `PhysicsStep` steps the world logs an error and hits nothing.
- A world without physics hits nothing.

Scripts call the same queries (`Raycast`, `RaycastAll`); see `docs/script-api-reference.md`.

## Debug drawing

The debug render key (`input::kDebugRenderKey`) toggles physics debug drawing in a world with a window. While it's on,
every body draws as a wireframe, and every `Raycast` / `RaycastAll` (C++ or script) draws:

- the ray from its origin to the nearest hit in green, and on past it to `max_distance` in gray; a miss draws its
  whole length in yellow;
- a small red cross at each hit point, and the surface normal there as a cyan line, 0.5 m long.

The lines are depth-tested 3D lines (the render module's `LineQueue`, see `engine/render/README.md`, "3D lines"), so
geometry in front hides them. `PhysicsDebugRender` queues them in `PreStore`, once per Frame. A frame shows the rays cast
since the previous frame; when none were, it keeps showing the last ones, so a one-off cast (a click, a single Tick)
stays visible until the next cast. Turning debug drawing off drops them.

## Several physics worlds in one process

Each Flecs world that imports `physics` gets its own `JoltBackend` (its own `PhysicsSystem`, bodies and characters),
so a client can run a server world beside its own. Worlds can be created and destroyed in any order, on one thread.

- **Jolt's process globals are set up once per process.** The allocator hooks, `Factory`, type registration and
  default physics material are global in Jolt. The first backend to `Initialize` sets them up, and they're torn down
  at process exit (a static guard's destructor), never when a world shuts down. So tearing down one world never pulls
  them out from under another, and a world created after every other is gone reuses them. That's how Jolt expects
  them used, and registering again after `UnregisterTypes` crashes on WASM.
- **Only a world that draws installs the debug renderer.** Jolt has one global `DebugRenderer` (`sInstance`), so the
  module creates the adapter and the `PhysicsDebugRender` / `PhysicsDebugRenderSync` systems only when the world's
  Platform reports `HasWindow()`. A headless world (a server, the tests) never creates or clears it. Two worlds with
  windows in one process aren't supported.
- **`ENABLE_PHYSICS_DEBUG_RENDERER`** (CMake option, default ON) compiles the engine's debug renderer out when OFF, so a
  headless program carries no physics rendering code. It strips the engine side only (gated by
  `ENGINE_PHYSICS_DEBUG_RENDERER` in `jolt/jolt_debug_config.hpp`). Jolt's own `JPH_DEBUG_RENDERER` comes from how vcpkg
  built the library (the `debugrenderer` feature, absent on Emscripten) and changes Jolt's class layouts, so it can't be
  switched off from the engine's CMake.

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

- PhysicsSyncBody (OnSet RigidBody — creates the body if CollisionShape is already present, or updates an existing one;
  see "Changing a body after creation")
- PhysicsSyncBodyOnCollisionShape (OnSet CollisionShape + has RigidBody — creates the body, or recreates it with a
  changed shape)
- PhysicsSetBodyTransform (OnSet WorldTransform + has RigidBody — moves the body to the seeded transform; setting a
  rigid body's `Transform` teleports it)
- PhysicsSyncToBackend (OnSet — syncs ECS transform changes to Jolt)
- PhysicsRemoveBody (OnRemove)

Systems run in `OnUpdate` (logic) or `OnStore` (debug rendering), scoped to `scene::GameScene` via
`.add<scene::GameScene>()` on the system entity, with proper ordering for the physics simulation pipeline.
