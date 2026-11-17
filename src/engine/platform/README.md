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
| `file_save_store.hpp/.cpp` | `FileSaveStore`, the file-per-slot save store desktop and headless share. |
| `debug_ui.hpp`, `imgui/` | The debug UI interface and its ImGui implementation (see `engine/debug/README.md`). |
| `render_batch.hpp/.cpp` | Groups `RenderCommand`s into instanced draws for `SubmitBatch`. |
| `gltf_materials.hpp/.cpp`, `cgltf_prefixed.hpp` | Reads glTF material flags raylib drops; the engine's prefixed cgltf copy. |

A new pure virtual on `Platform` must be implemented by both `RaylibBackend` and `HeadlessPlatform`.

## Lines

| Call | Space | Depth | Thickness |
|---|---|---|---|
| `DrawLine(a, b, thickness, color)` | screen pixels (2D) | drawn over everything | yes |
| `DrawLine3D(from, to, color)` | world, between `BeginMode3D` and `EndMode3D` | depth-tested: geometry in front hides it | none: one pixel on every target |

`DrawLine3D` takes no thickness because no target honors one: desktop GL core profiles ignore `glLineWidth` above 1,
and WebGL implementations cap it at 1. It's unlit and ignores the material and lighting state. Game code usually
pushes onto the render module's `LineQueue` instead of calling it directly (`engine/render/README.md`, "3D lines"),
since a game's own `OnStore` system runs after the 3D pass has closed.

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
| Saves | files in `<exe dir>/saves` (desktop), `localStorage` (web) | files in the directory given at construction (default `<exe dir>/saves`), or none |

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

## Save store

Desktop and headless keep saves the same way, through `FileSaveStore`: each slot is `<directory>/<name>.json`. A
slot name must not be empty or hold `/`, `\` or `..` (an invalid name writes nothing and reads as missing), and
`ListSaves` returns the slot names sorted. The directory is created on the first write.

A write is atomic and durable: the data goes to `<name>.json.tmp`, is flushed to the disk (`_commit` on Windows,
`fsync` elsewhere, which also flushes the directory after the rename), and is then renamed over the slot. A crash or
power loss mid-write leaves the previous save intact. A leftover `.tmp` file isn't a slot; the next write to that
slot replaces it. The web game's `localStorage` save is a single `setItem`, atomic in the browser but with no
durability promise beyond what the browser gives.

`HeadlessPlatform` takes its save directory as its second argument. It defaults to `DefaultSaveDirectory()`,
where desktop saves (`<exe dir>/saves`), so a dedicated server and the game it ships beside share their slots.
A test points it at a temporary directory, and `HeadlessPlatform::kNoSaves` turns persistence off: writes are
dropped and every slot reads as missing.

On the web, `DefaultSaveDirectory()` is `/saves` in Emscripten's in-memory filesystem. A headless Platform there
keeps its slots in memory for the page's lifetime; they are not the web game's `localStorage` slots, and neither
side sees the other's.

```cpp
auto server = std::make_unique<engine::platform::HeadlessPlatform>(assetDirectory, "/var/lib/mygame/saves");
auto scratch = std::make_unique<engine::platform::HeadlessPlatform>(".", engine::platform::HeadlessPlatform::kNoSaves);
```

Tests subclass `HeadlessPlatform` to make a few calls observable, such as a texture load that succeeds
(`tests/unit/path_asset_test.cpp`) or a Platform that claims a window (`tests/unit/two_physics_worlds_test.cpp`).
