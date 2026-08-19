# Render System

Data-driven 3D rendering for Flecs ECS. An entity is drawn by combining an
`ecs::WorldTransform` with a shape component and a `Material`; the render systems read the world matrix and issue draw
calls through the `platform::Platform` interface (never Raylib directly).

## Lighting, shadows and textures

Primitives are drawn as **lit meshes**, not flat immediate-mode shapes. The platform caches unit meshes (cube, sphere,
plane, cylinder) that carry normals + texture coordinates, and draws them through a Phong shader
(`assets/shaders/glsl330/lit.*`, with a `glsl100` variant for WebGL). This is what gives depth cues: faces at different
angles receive different brightness.

- **Phong shading** — ambient + one directional light + specular, configured from ECS light components (`AmbientLight`,
  `DirectionalLight`).
- **Shadows** — cheap **planar projected shadows**: shadow-casting primitives are re-drawn flattened onto the ground
  plane (`DirectionalLight.shadow_ground_y`) along the light direction. Ideal for a flat playfield; no render targets,
  so it works on desktop and WebGL alike.
- **Textures** — an optional `AlbedoMap { path, handle }` component is resolved to a platform texture handle when the
  path is set (event-driven `OnSet` observer). Render systems call
  `entity.try_get<AlbedoMap>()` to get the handle; `Material` no longer carries `texture_handle`. Multiple texture types
  (NormalMap, RoughnessMap, …) follow the same pattern — each is a distinct Flecs component, registered via the shared
  `RegisterTextureResolver<T>` template.

If the lit shader fails to load, the platform falls back to unlit immediate-mode drawing.

## Structure

- **render_components.hpp** — plain-data components: `CubePrimitive`, `SpherePrimitive`,
  `QuadPrimitive`, `CapsulePrimitive`, `MeshPrimitive`, `Material`, `AlbedoMap`, the lights (`AmbientLight`,
  `DirectionalLight`), and the `Camera`.
- **render_module.hpp/cpp** — reflection, scripting registration, the `CameraTransformUpdate`,
  `ResolveAlbedoMap` and `ResolveMeshPrimitive` observers, and the render systems.
- **render.hpp** — public umbrella header.

## Usage

```cpp
// Registers reflection, the camera observer, and all render systems.
engine::render::RenderModule render_module(world); // or world.import<render::RenderModule>();
```

The module registers (all in the `OnStore` phase, scoped to `scene::GameScene` via `.add<scene::GameScene>()` on each
system entity, in order):

- `Render3DBegin` — finds the active `Camera` (+ `ecs::Transform`) and opens the 3D pass.
- `RenderLightingUpload` — pushes the first `DirectionalLight` + the `AmbientLight` singleton to the lit shader (Phong +
  shadow parameters).
- `RenderCubes`, `RenderSpheres`, `RenderQuads`, `RenderCapsules`, `RenderMeshes` — draw each matching entity using its
  `WorldTransform` matrix and `Material`.
- `Render3DEnd` — closes the 3D pass (balanced against `Render3DBegin`).

Plus three `OnSet` observers: `CameraTransformUpdate` recomputes the camera view/projection matrices when its
`ecs::Transform` changes; `ResolveAlbedoMap` loads an `AlbedoMap`'s image once and stores the platform handle back on
the component (texture-map types are registered through the shared `RegisterTextureMapObserver<T>` template); and
`ResolveMeshPrimitive` loads a `MeshPrimitive`'s model once and stores its handle, so the draw systems never load
lazily.

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

e.set<engine::render::Material>({.color = {200, 120, 60, 255}, .wireframe = false, .cast_shadow = true});

// Optional: give it a diffuse texture (its handle is resolved on the component by ResolveAlbedoMap).
e.set<engine::render::AlbedoMap>({.path = dataDir + "/textures/checker.png"});
```

The shape component describes the base primitive; the `WorldTransform` matrix (position, rotation, and scale) is applied
on top, so scaling an entity scales its primitive.

## Lighting

Add an `AmbientLight` singleton and one `DirectionalLight` entity. The first `DirectionalLight`
drives Phong shading and (when `casts_shadows`) planar shadows onto `shadow_ground_y`:

```cpp
world.set<engine::render::AmbientLight>({.color = {90, 105, 130, 255}, .intensity = 0.35F});

auto sun = world.entity("SunLight");
sun.set<engine::render::DirectionalLight>({
    .direction = {-0.55F, -1.0F, -0.4F}, // direction the light travels
    .color = {255, 245, 220, 255},
    .intensity = 1.0F,
    .casts_shadows = true,
    .shadow_ground_y = 0.25F,             // top of the playfield
});
```

`Material.cast_shadow` opts an individual primitive out of casting (e.g. the floor itself, which only *receives*
shadows). Because lights are plain components, scripts can mutate them live — see
`assets/scripts/sun_cycle.as`, which hue-cycles the sun colour each tick.

## Camera

Create one entity with a `Camera` and an `ecs::Transform` (the transform position is the eye point). The first such
entity found drives the 3D pass:

```cpp
auto cam = world.entity("Camera");
cam.set<engine::ecs::Transform>({{0.0F, 5.0F, 5.0F}});
cam.set<engine::render::Camera>({.target = {0.0F, 1.0F, 0.0F}, .fov = 60.0F});
```

## Flecs registration patterns

1. **Per-entity systems** (`.each()`): `RenderCubes`, `RenderSpheres`, `RenderQuads`,
   `RenderCapsules`, `RenderMeshes`.
2. **Global systems** (`.run()` with singleton access): `Render3DBegin`, `Render3DEnd`
   (they read/write the internal `Render3DState` and the cached camera query).
3. **Observers** (event-based): `CameraTransformUpdate`, `ResolveAlbedoMap`, `ResolveMeshPrimitive` (all `OnSet`).
