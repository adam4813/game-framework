# Platform

The one seam between the engine and the outside world: window, input, drawing, assets, audio and save data. Every
Raylib (and ImGui) call lives behind `platform::Platform` (ADR 0001), so game and engine logic use only its interface
and POD types (`core::Rect`, `core::Rgba`, glm).

Every world has a Platform. `EngineContext` publishes it as the `PlatformRef` singleton, which is never null, and code
reaches it with `world.get<platform::PlatformRef>().ptr`.

## Structure

| File | Purpose |
|---|---|
| `platform.hpp` | The `Platform` interface, `PlatformRef`, and the POD types its calls take (cameras, materials, lights, render commands). |
| `raylib/raylib_backend.hpp/.cpp` | `RaylibBackend`, the windowed Platform for desktop and the web. |
| `headless_platform.hpp` | `HeadlessPlatform`, the Platform with no window. |
| `debug_ui.hpp`, `imgui/` | The debug UI interface and its ImGui implementation (see `engine/debug/README.md`). |
| `render_batch.hpp/.cpp` | Groups `RenderCommand`s into instanced draws for `SubmitBatch`. |
| `gltf_materials.hpp/.cpp`, `cgltf_prefixed.hpp` | Reads glTF material flags raylib drops; the engine's prefixed cgltf copy. |

A new pure virtual on `Platform` must be implemented by both `RaylibBackend` and `HeadlessPlatform`.

## Two Platforms

| | `RaylibBackend` | `HeadlessPlatform` |
|---|---|---|
| Used by | games (`main.cpp` creates it) | servers, tools, tests; an `EngineContext` created with no Platform |
| `HasWindow()` | `true` | `false` |
| `AssetDirectory()` | `<exe dir>/assets` (desktop), `/assets` (web) | the directory given at construction (default `.`) |
| Draws, audio, window calls | real | no-ops |
| Input | the keyboard and mouse | nothing pressed, mouse at the origin |
| Loads (`LoadMesh`, `LoadTexture`, `LoadSound`, ...) | handles | `-1` |
| `DeltaTime()` | the measured frame time | a fixed 1/60 s |

`HasWindow()` is how code tells a world that draws from one that doesn't. Code that sets up rendering-only state
checks it: the physics module installs Jolt's debug renderer only when it's true (`engine/physics/README.md`).

## Headless worlds

A world with no window, such as a server beside a client in the same process, runs on a `HeadlessPlatform` given its
asset directory:

```cpp
engine::EngineContext server{std::make_unique<engine::platform::HeadlessPlatform>(assetDirectory)};
```

`assets::ResolveAsset` then resolves paths against that directory, so the world loads levels, mesh colliders (built
from glTF on the CPU, no GPU needed) and other files by asset path, the same strings the windowed game uses. A
headless loop passes its own measured dt to `fixed_step::RunFrame(world, dt)`, rather than reading `DeltaTime()`.

Tests subclass `HeadlessPlatform` to make a few calls observable, such as a texture load that succeeds
(`tests/unit/path_asset_test.cpp`) or a Platform that claims a window (`tests/unit/two_physics_worlds_test.cpp`).
