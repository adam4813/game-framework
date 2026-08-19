# Asset System

Unified, reference-counted asset ownership over the platform's texture/sound/mesh loaders.

Replaces the per-module ad-hoc handle maps (the old `audio::AudioAssets`, the render texture/mesh resolvers loading
straight from the platform) so loading, deduplication, reference-counting and unloading all live in one place.

## Structure

- **assets_components.hpp** — `AssetType` enum, `AssetRecord`, and the `AssetRegistry` singleton.
- **assets_module.hpp/cpp** — `AssetModule` (owns the registry) plus the free `Acquire` / `Get` /
  `Release` / `RegisterAlias` / `Find` API and the `LoadTexture` / `LoadSound` / `LoadMesh`
  convenience wrappers.
- **assets.hpp** — Public umbrella header.

## Usage

```cpp
// Imported by EngineContext before the modules that consume assets (audio, render).
world.import<engine::assets::AssetModule>();

// Load (or reuse) a texture; the handle is cached and reference-counted.
const int handle = engine::assets::LoadTexture(world, assetDir + "/textures/checker.png");

// Bind a stable id to a path for id-based lookup elsewhere.
engine::assets::RegisterAlias(world, "ui_click", engine::assets::AssetType::Sound, clickPath);
const int click = engine::assets::Find(world, "ui_click");

// Drop a reference; the platform resource is freed when the last owner releases it.
engine::assets::Release(world, engine::assets::AssetType::Texture, path);
```

## Behavior

- **Deduplication** — the same `"<type>:<path>"` always resolves to a single platform handle.
- **Reference counting** — `Acquire` / `RegisterAlias` increment; `Release` decrements and unloads through the platform
  when the count hits zero. Handles are never reused, so a freed slot stays a safe no-op.
- **Empty paths** return `-1` without touching the registry.

## Registered systems

None — this is a data/service module. It only sets the `AssetRegistry` singleton; all access is through the free
functions above.
