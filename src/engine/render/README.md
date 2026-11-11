# Render System

Data-driven 3D rendering for Flecs ECS. An entity is drawn by combining an
`ecs::WorldTransform` with a shape component and a `RenderWith` relationship pointing to a **material entity**; the
render systems resolve the world matrix and material, then issue draw calls through the `platform::Platform` interface
(never Raylib directly).

## Relationships and Material Entities

Each renderable entity links to a named **material entity** via the `(RenderWith, mat_entity)` relationship. The
material entity carries all surface data: `Material` (colour, wireframe, cast-shadow, unlit and double-sided flags; wireframe is desktop-only,
since WebGL has no polygon wire mode), optionally `AlbedoMap`
(diffuse texture), and optionally `ShaderMap` (custom GLSL shader stages). This mirrors AAA engine conventions where
materials own their texture references and multiple objects can share the same material entity.

Materials can be authored declaratively in JSON levels as nested children with a `"link": "render_with"`:

```json
{
  "name": "Floor",
  "components": {
    "transform": {"position": [0, 0, 0]},
    "cube": {"size": [1.0, 1.0, 1.0]}
  },
  "children": [
    {
      "link": "render_with",
      "components": {
        "material": {"color": [235, 240, 235, 255], "cast_shadow": false},
        "albedo": {"path": "textures/checker.png"}
      }
    }
  ]
}
```

The `link: "render_with"` tells the level loader to wire the parent→child relationship. The render module's
`LinkLoader` registers the factory that does this.

In C++, the same pattern:

```cpp
// Create a named material (child of the scene root for auto-cleanup)
auto mat = world.entity("Material::Floor")
    .child_of(sceneRoot)
    .set<render::Material>({.color = {235, 240, 235, 255}, .cast_shadow = false})
    .set<render::AlbedoMap>({.path = "textures/checker.png"});

// Declare what the object renders with
floor.set<render::CubePrimitive>({.size = {1.0F, 1.0F, 1.0F}})
     .add<render::RenderWith>(mat);
```

`RenderWith` is registered with `flecs::Exclusive` so each entity can target at most one material. The `.self().up()`
flags on the render system query also match entities whose ChildOf ancestor has `RenderWith`, enabling parent-inherited
materials.

### ScriptTraversal — transparent script access

`Material`, `AlbedoMap`, and `ShaderMap` are tagged with `(ScriptTraversal, RenderWith)` on their component entities.
This tells the scripting backend to navigate through `RenderWith` when `GetMaterial()` / `GetAlbedoMap()` etc. are
called from a script and the component is not found directly on the host entity — allowing scripts to work without any
code changes regardless of where the material data lives.

## Camera and LookAt Relationship

A camera entity carries an `ecs::Transform` (for position/rotation) and an optional `(LookAt, target_entity)`
relationship.

- If `LookAt` is set, `Render3DBegin` aims the camera at the target entity's `WorldTransform.position`.
- If not set, the camera looks forward along its own rotation.

Matrices are computed **on-demand** each frame in `Render3DBegin` from the camera's `WorldTransform` and its `LookAt`
target. There is no `CameraTransformUpdate` observer caching old values — the physics debug renderer always reads fresh
matrices.

Declare a camera with a target in JSON via the `refs` field:

```json
{
  "name": "Camera",
  "components": {
    "transform": {"position": [0.0, 5.0, 5.0]},
    "camera": {"fov": 60.0, "aspect_ratio": 1.7777778}
  },
  "refs": {
    "look_at": "CameraTarget"
  }
}
```

The level loader resolves `"look_at": "CameraTarget"` in a second pass and wires `LookAt(camera → CameraTarget)` once
all named entities are built.

## Lighting, Shadows, and Textures

Primitives are drawn as **lit meshes** through a Phong shader (`assets/shaders/glsl330/lit.*`, with a `glsl100`
variant for WebGL).

- **Phong shading** — ambient + one directional light + specular, configured from `AmbientLight` and `DirectionalLight`.
- **Point lights** — a `point_light` entity (`PointLight { color, intensity, constant, linear, quadratic }`, all
  optional in level JSON) lights primitives, models and instanced batches from its `WorldTransform` position, on top of
  the ambient and directional terms. Attenuation is `1 / (constant + linear·d + quadratic·d²)`; the defaults fade over
  about 50 m. At most **4** are used (`LightParams::kMaxPointLights`, in query order); the rest are ignored with one
  logged warning. Point lights cast no shadows. A scene with none renders as before.
- **Skinned models** — a `mesh` entity that also has a `skeletal_animation_set` is drawn by `RenderSkinnedMeshes`
  (and skipped by `RenderMeshes`) through `Platform::DrawSkinnedMesh`, at its `AnimationPlayer`'s clip and time (clip 0
  at time 0 without a player). See `src/engine/animation/README.md`.
- **Shadows** — cheap **planar projected shadows** flattened onto `DirectionalLight.shadow_ground_y`. No render targets,
  so it works on desktop and WebGL alike.
- **Unlit** — `Material.unlit` draws the colour exactly as given, skipping the scene light (flat white art, markers).
  It is independent of `cast_shadow`, which only controls the projected shadow.
- **Double-sided** — `Material.double_sided` (`"double_sided": true` in a level's `material`) turns back-face culling
  off for that draw, on every path (primitives, instanced batches, models, skinned models, dynamic meshes). Use it for
  foliage cards, flags and banners, grates, ribbons, and open geometry whose inside shows; billboards and particles
  always face the camera and don't need it. The lit shaders flip a back face's normal (`gl_FrontFacing`), so each side
  is lit by the lights on its own side. The planar shadow pass stays culled. A model's glTF `materials[].doubleSided`
  (Blender: material **Backface Culling** off) makes the meshes using that material double-sided, read with cgltf at
  load since raylib's loader drops it. The flags combine: `true` on the entity's `Material` makes the whole model
  double-sided, but `false` can't turn off an asset's flag; fix that in the asset. Alpha-cutout surfaces are fine;
  **blended** double-sided surfaces draw their two faces in no fixed order and can show sorting artifacts. Desktop and
  WebGL alike. Sandbox example: `DoubleSidedSign` beside a culled `SingleSidedSign` in `assets/levels/demo.level.json`.
- **Textures** — `AlbedoMap { path, handle }` on the material entity is resolved to a platform handle via the
  `RegisterPathAsset<AlbedoMap>()` observer. `ShaderMap { stages, handle }` similarly loads a custom GLSL shader.
- **Runtime textures** — an `AlbedoMap` with an empty `path` keeps the `handle` it is set with, so a lit mesh can show
  a texture the game made with `Platform::UploadTexture` and keeps changing with `UpdateTexture`
  (`e.set<AlbedoMap>({.path = "", .handle = tex})`). The game owns that handle: removing the component frees nothing,
  so call `UnloadTexture` yourself. To swap a file albedo for an uploaded one, `remove<AlbedoMap>()` first, then set
  it; setting over the existing component would never release the file's reference. Sandbox example: the
  `RuntimeAlbedoQuad` in `CubeScene` (`games/sandbox/src/game/cube_scene.cpp`).

## Structure

- **render_components.hpp** — plain-data components: shape primitives, `Material`, `AlbedoMap`, `ShaderMap`,
  `RenderWith` (relationship tag), `LookAt` (relationship tag), lights (`AmbientLight`, `DirectionalLight`, `PointLight`), and
  `Camera`.
- **render_module.hpp/cpp** — reflection, scripting registration (`ScriptTraversal` tagging), observers, systems.
- **render.hpp** — public umbrella header.

## Primitive Rendering Template

`RegisterPrimitiveRenderer<TPrim>(world, name, draw)` is a generic template that registers a system for any shape type
`TPrim`. The system queries entities with `TPrim`, resolves the material via `(RenderWith, *)` `.self().up()`, and
issues the draw call:

```cpp
// Register renderers for all primitives
RegisterPrimitiveRenderer<engine::render::CubePrimitive>(
    world, "RenderCubes",
    [platform](const auto& prim, const auto& wt, const auto& mat) {
        platform->DrawCube(wt.matrix, prim.size, mat.color);
    });
```

## Path-Asset Resolution Template

`RegisterPathAsset<T>(world, AssetType)` is a unified template for resolving `path` → `handle` for any component with
those fields. It registers:

- `OnSet` observer — calls `Acquire(path)` via the asset registry, stores the returned handle.
- `OnRemove` observer — calls `Release(path)` to decrement the ref count.

An empty `path` means "a handle you own": `OnSet` keeps the given `handle` and acquires nothing, and `OnRemove`
releases nothing. Replacing a path-loaded component with an owned handle is remove, then set (see "Runtime textures"
above).

Used for `AlbedoMap`, `MeshPrimitive`, `SoundEffect`, `Music` and `SkeletalAnimationSet`.

## Adding a renderable to an entity

```cpp
auto e = world.entity();
e.set<engine::ecs::WorldTransform>({/* position/rotation/scale + matrix */});

// Pick exactly one shape:
e.set<engine::render::CubePrimitive>({.size = {1.0F, 1.0F, 1.0F}});
// e.set<engine::render::SpherePrimitive>({.radius = 0.5F});
// e.set<engine::render::QuadPrimitive>({.size = {2.0F, 2.0F}});
// e.set<engine::render::CapsulePrimitive>({.radius = 0.5F, .height = 1.0F});
// e.set<engine::render::MeshPrimitive>({.path = dataDir + "/models/ball.obj"});

// Create a material entity (child of scene root for auto-cleanup)
auto mat = world.entity("Material::MyObject")
    .child_of(sceneRoot)
    .set<engine::render::Material>({.color = {200, 120, 60, 255}, .wireframe = false, .cast_shadow = true});

// Optional: diffuse texture and/or custom shader on the material entity
mat.set<engine::render::AlbedoMap>({.path = dataDir + "/textures/checker.png"});
// mat.set<engine::render::ShaderMap>({.stages = {{.type="vertex", .path="custom.vs"}, ...}});

// Link the renderable to its material
e.add<engine::render::RenderWith>(mat);
```

The shape component describes the base primitive; the `WorldTransform` matrix is applied on top.

### Many entities, each with its own colour (hordes, tile grids, hit flash)

Put a plain `Material` on each entity instead of a `RenderWith` target. The cube, sphere and quad enqueuers read an
inline `Material` as a table column (the fast path), and the platform still instances every command with the same mesh
into one draw call, because colour is per instance. A unique material entity per renderable puts every entity in its own
table and falls back to a per-entity relationship lookup. To tint one instance (a hit flash), change its `Material.color`
in place.

```cpp
// A crab: a root with a Transform, and child spheres that each carry their own tint.
world.entity().child_of(crab)
    .set<spatial::Transform>({.position = {0.85F, 0.5F, 0.7F}})
    .set<render::SpherePrimitive>({.radius = 0.32F})
    .set<render::Material>({.color = {216, 69, 43, 255}});
```

Working example: `games/crabnado/src/horde.cpp`. `RenderStats` (`queued`, `draw_calls`, `instances`) confirms the
batching at runtime.

