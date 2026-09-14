# Render System

Data-driven 3D rendering for Flecs ECS. An entity is drawn by combining an
`ecs::WorldTransform` with a shape component and a `RenderWith` relationship pointing to a **material entity**; the
render systems resolve the world matrix and material, then issue draw calls through the `platform::Platform` interface
(never Raylib directly).

## Relationships and Material Entities

Each renderable entity links to a named **material entity** via the `(RenderWith, mat_entity)` relationship. The
material entity carries all surface data: `Material` (colour, wireframe, cast-shadow flag), optionally `AlbedoMap`
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
- **Shadows** — cheap **planar projected shadows** flattened onto `DirectionalLight.shadow_ground_y`. No render targets,
  so it works on desktop and WebGL alike.
- **Textures** — `AlbedoMap { path, handle }` on the material entity is resolved to a platform handle via the
  `RegisterPathAsset<AlbedoMap>()` observer. `ShaderMap { stages, handle }` similarly loads a custom GLSL shader.

## Structure

- **render_components.hpp** — plain-data components: shape primitives, `Material`, `AlbedoMap`, `ShaderMap`,
  `RenderWith` (relationship tag), `LookAt` (relationship tag), lights (`AmbientLight`, `DirectionalLight`), and
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
- `OnRemove` observer — calls `Release(handle)` to decrement the ref count.

Used for `AlbedoMap`, `MeshPrimitive`, and `SoundEffect`.

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

