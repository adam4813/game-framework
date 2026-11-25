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

Primitives are drawn as **lit meshes** through a Phong shader (`assets/shaders/lit.*`), the same source on desktop and
the web (see "Shaders" below).

- **Phong shading** — ambient + one directional light + specular, configured from `AmbientLight` and `DirectionalLight`.
- **Point lights** — a `point_light` entity (`PointLight { color, intensity, constant, linear, quadratic, range }`, all
  optional in level JSON) lights primitives, models (skinned ones too), instanced batches and translucent draws from its
  `WorldTransform` position, on top of the ambient and directional terms. Any number; see "Many point lights" below.
  Point lights cast no shadows.
- **Loaded models** — a `mesh` entity queues one `RenderCommand` holding its model handle, and `Render3DFlush` draws
  all copies of a model that share a texture and material flags in one instanced draw per sub-mesh (plus one for
  shadows). Tint is per instance, so differently colored copies still batch. Sub-meshes keep their authored texture,
  color, vertex colors and double-sided flag. Models with a `ShaderMap` or `wireframe` draw directly, one call per
  sub-mesh, before the batch.
- **Model culling** — a static opaque model outside the camera frustum isn't drawn, and neither is its shadow (the
  primitives' rule). `Platform::MeshBounds` gives its bounding sphere, computed at load; an entity's own
  `FrustumCullRadius` replaces it, so a large one keeps an off-screen caster's shadow.
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
  WebGL alike. Sandbox example: `DoubleSidedSign` beside a culled `SingleSidedSign` in `games/sandbox/assets/levels/demo.level.json`.
- **Textures** — `AlbedoMap { path, handle }` on the material entity is resolved to a platform handle via the
  `RegisterPathAsset<AlbedoMap>()` observer. `ShaderMap { stages, handle }` similarly loads a custom GLSL shader.
- **Runtime textures** — an `AlbedoMap` with an empty `path` keeps the `handle` it is set with, so a lit mesh can show
  a texture the game made with `Platform::UploadTexture` and keeps changing with `UpdateTexture`
  (`e.set<AlbedoMap>({.path = "", .handle = tex})`). The game owns that handle: removing the component frees nothing,
  so call `UnloadTexture` yourself. To swap a file albedo for an uploaded one, `remove<AlbedoMap>()` first, then set
  it; setting over the existing component would never release the file's reference. Sandbox example: the
  `RuntimeAlbedoQuad` in `CubeScene` (`games/sandbox/src/game/cube_scene.cpp`).

## Many point lights

Point lights are drawn with clustered forward shading (ADR 0009): a scene can author any number, and a fragment pays
only for the lights that reach its part of the view. Lights outside the camera's view cost nothing on the GPU.

**Give a light a `range`.** Attenuation, `1 / (constant + linear·d + quadratic·d²)`, never reaches zero by itself, so the
light also falls off smoothly to zero at `range` world units. A small range is what keeps many lights cheap. Left at 0
(the default), the range is derived as the distance where attenuation times intensity falls to 1/256: about 88 m for
the default terms, and never more than 1000 m (`EffectiveRange`, `point_light_range.hpp`).

```json
{
  "name": "Lamp",
  "components": {
    "transform": {"position": [2.0, 3.0, 0.0]},
    "point_light": {"color": [255, 180, 120, 255], "intensity": 1.5, "range": 6.0}
  }
}
```

**Budgets** (`engine/platform/light_clusters.hpp`) apply to what is in view, not to what is authored: 512 visible lights
per Frame (`kMaxVisibleLights`) and 64 per cluster (`kMaxLightsPerCluster`). Over either, the most important lights are
kept, by intensity over squared distance to the camera, with ties broken by entity id so a still view doesn't flicker.
Each budget logs a warning the first time it is exceeded. A cluster over its budget drops lights its neighbors may keep,
so a seam can show along a tile's edge: use smaller ranges there.

**How it works.** Each Frame `RenderLightingUpload` hands every `PointLight` to `Platform::SetLighting`. The backend calls
`BuildLightClusters`, which culls the lights to the camera and lists them per cluster: 16 x 9 screen tiles by 24 depth
slices, exponential in depth from 2 m to 250 m, with the first and last slices covering everything nearer and farther.
The lights, the cluster ranges and the light indices go up as three RGBA32F textures, on texture units 12 to 14 (raylib
binds material maps to 0 to 11), and `lighting.glsl` reads them with `texelFetch`.

**Cost.** `RenderStats` and the `render.lights.*` profiler plots hold the light counts, the dropped counts and the CPU
time of the cluster build. Frame times of the sandbox's Many Lights scene at 1280x720, measured 2026-11-23:

| Lights in view | Desktop (RTX 3080 Ti, uncapped) | Web (SwiftShader, so all on the CPU) |
|---|---|---|
| 4 | 0.55-0.71 ms | 36.4 ms |
| 64 | 0.57-0.74 ms | 43.9 ms |
| 256 | 0.65-0.82 ms | 73.7 ms |
| 512 | 0.76-0.95 ms | 113.4 ms |
| 256, plus 2000 out of view | 0.69-0.85 ms | 74.0 ms |

The desktop column is the whole Frame over two runs, which differ by about 0.15 ms; within a run the 2000 lights out of
view add 0.02 to 0.04 ms of CPU (culling them and their markers). The web build in a GPU-backed browser on the same
machine holds the page's 16.7 ms at every count. The cluster build takes about 0.1 ms for 256 lights and 0.2 to 0.3 ms
for 512.

Sandbox example: `LightsScene` (`games/sandbox/src/game/lights_scene.cpp`,
`games/sandbox/assets/levels/lights_demo.level.json`).

## Shaders

A shader file serves desktop (GLSL 330) and the web (GLSL 300 es) alike: the backend prepends the target's `#version`
and precision lines, so the file has neither, and expands `#include "file"` lines, found next to the including file
(`engine/platform/shader_source.hpp`). Write what both dialects accept: `in`/`out`, `texture`, float literals with a
decimal point. `#ifdef GL_ES` separates them where it must. A file that starts with its own `#version` is compiled as it
is.

A custom `ShaderMap` shader opts in to the scene lighting by including `lighting.glsl` and calling
`SceneLighting(worldPosition, normal)`; the backend feeds its uniforms. `lit.vs` supplies `fragPosition` and
`fragNormal`:

```glsl
#include "lighting.glsl"
in vec3 fragPosition;
in vec3 fragNormal;
out vec4 finalColor;
void main() {
	finalColor = vec4(SceneLighting(fragPosition, normalize(fragNormal)), 1.0);
}
```

## 3D lines

To draw a line between two world points (a link between buildings, a laser, a debug ray), push a `Line3D` onto the
`LineQueue` singleton from a scene's `Tick`, or from a per-Frame, single-threaded system that runs before `OnStore`.
Not from a FixedStep system: a Frame runs zero or several Ticks, so its line would flicker or draw twice. Not from a
`multi_threaded` system: the push is unsynchronized. Scripts call `DrawLine3D(from, to, color)`, which pushes for
them (`docs/script-api-reference.md`, "Render").

```cpp
world.get_mut<render::LineQueue>().lines.push_back(
    {.from = a.position, .to = b.position, .color = {40, 220, 255, 255}});
```

`Render3DLines` draws the queue inside the 3D pass through `Platform::DrawLine3D`, depth-tested against the scene, so
geometry in front of a line hides it. It runs before `Render3DFlush` and the lines write depth like opaque geometry,
so an occluder-fade ghost or a `Translucent` entity in front of a line blends over it. It then clears the queue: a line
shows for one frame, so push it every frame you want it seen. With no camera there's no 3D pass and the queue is
cleared undrawn. Lines are one pixel wide and unlit on desktop and WebGL alike; there's no thickness
(`engine/platform/README.md`, "Lines"). Sandbox example: `CubeScene` draws a line behind the falling cube and one that
slopes through the floor (`games/sandbox/src/game/cube_scene.cpp`).

## Translucent pass

Add the `Translucent` tag (`"translucent": {}` in level JSON) to draw an entity see-through, by the alpha of its
`Material.color`. It works on every shape: cube, sphere, quad, capsule, `MeshPrimitive` models (skinned ones too) and
`DynamicMesh`. Use it for placement previews (a translucent copy of a model), glass, ghosts and force fields.

```json
{
  "name": "PlacementPreview",
  "components": {
    "transform": {"position": [2.0, 0.0, 0.0]},
    "mesh": {"path": "models/robot.glb"},
    "material": {"color": [120, 200, 255, 110], "unlit": true},
    "translucent": {}
  }
}
```

A `Translucent` entity skips the opaque passes (the instanced batch, `RenderMeshes`, `RenderSkinnedMeshes`,
`RenderDynamicMeshes`, `RenderCapsules`). `Render3DTranslucent` draws it after all opaque geometry and the occluder
ghost, alpha-blended, with depth test on and depth write off, so it never hides what draws after it. The pass sorts its
entities back to front by the distance from the camera to each entity's `WorldTransform` position, so overlapping
translucent objects composite in the right order. `Material.unlit` gives the flat color a preview usually wants; a tint
and an albedo work as on the opaque path. Removing the tag returns the entity to the opaque passes.

Limits:

- **Per-object sort, not per-triangle.** Two translucent objects that intersect, or whose centers sort the wrong way
  round (a long object beside a small one), can blend in the wrong order where they overlap. One concave model seen
  through itself can show its far faces over its near ones; a capsule's body and caps blend over each other at the
  seams.
- **No instancing.** Each translucent entity is its own draw call, so keep the count small (tens, not thousands).
- **No frustum cull** except an entity's own `FrustumCullRadius`.
- **Shadows** still draw for a translucent object with `cast_shadow`, as solid as an opaque one's; turn `cast_shadow`
  off for a ghostly preview.

Sandbox example: `TranslucentScene` (`games/sandbox/src/game/translucent_scene.cpp`,
`games/sandbox/assets/levels/translucent_demo.level.json`).

### Occluder fade

`TransparentPass` is a different thing: the occluder-fade ghost for a `DynamicMesh` drawn with the fade-aware
`cutout.fs`. Set an `OcclusionFade` singleton and the cutout punches a hole where it covers the camera's `LookAt` target
in the opaque pass, then `Render3DFlush` redraws the covering fragments as a faint, depth-write-off ghost. The
`TranslucentScene` wall shows it.

## Structure

- **render_components.hpp** — plain-data components: shape primitives, `Material`, `AlbedoMap`, `ShaderMap`,
  `RenderWith` (relationship tag), `LookAt` (relationship tag), lights (`AmbientLight`, `DirectionalLight`, `PointLight`),
  `Camera`, and the `Translucent` and `TransparentPass` tags.
- **point_light_range.hpp** — the distance at which a `PointLight` ends.
- **translucent_pass.hpp** — the translucent pass's draw record and its back-to-front sort.
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

