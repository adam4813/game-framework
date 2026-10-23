# Game Framework — C++23 / Raylib / Flecs 4.x ECS

A small, data-driven game engine built on **Flecs 4.x** (ECS), **Raylib** (platform/rendering), **Jolt** (physics),
**AngelScript** (scripting), and **nlohmann_json** (data). Everything is a module that registers into a single Flecs
world; game behavior is expressed as **components + systems + data**, not hardcoded branches.

- **Cross-platform**: Desktop (Windows/macOS/Linux) and WebAssembly
- **Modular architecture**: Each subsystem (scene, assets, audio, input, physics, render, UI, etc.) is a Flecs module
- **Data-driven**: Game content lives in JSON configs, not C++ branches
- **ECS-first**: Composition over inheritance, systems over virtual methods

## Quick Start

### Prerequisites

You need:

- **CMake** 3.30+
- **Ninja** build system
- **VCPKG** for dependency management (minimum version: `dd3097e305afa53f7b4312371f62058d2e665320`)
- **EMSDK** 4.0.10+ for WebAssembly builds (optional for desktop-only)

### Installation

#### 1. Set up VCPKG

VCPKG manages C++ dependencies (Raylib, Flecs, Jolt, etc.).

```bash
# Clone VCPKG if you haven't already
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # macOS/Linux: ./bootstrap-vcpkg.sh

# Set the environment variable to point to your VCPKG installation
export VCPKG_ROOT=/path/to/vcpkg  # or setx VCPKG_ROOT on Windows
```

The project uses VCPKG baseline hash `dd3097e305afa53f7b4312371f62058d2e665320`. The baseline is specified in
`vcpkg.json` and ensures reproducible dependency versions across builds.

#### 2. Set up EMSDK (for WebAssembly builds only)

If you plan to build for the web, install Emscripten SDK:

```bash
# Clone EMSDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 4.0.10
./emsdk activate 4.0.10

# Set the environment variable
export EMSDK=/path/to/emsdk  # or setx EMSDK on Windows
```

Verify the installation:

```bash
emcc --version
```

### Building

All builds use CMake presets. Make sure `VCPKG_ROOT` (and `EMSDK` for web builds) are set in your environment.

#### Desktop Build

```bash
# Configure for Desktop
cmake --preset desktop -B build/desktop

# Build release
cmake --build --preset desktop-release
# or debug
cmake --build --preset desktop-debug
```

The executable is at `build/desktop/Release/sandbox` (or `Debug/sandbox`).

#### WebAssembly Build

```bash
# Configure for WASM
cmake --preset wasm -B build/wasm

# Build release
cmake --build --preset wasm-release
# or debug
cmake --build --preset wasm-debug
```

### Running

#### Desktop

Simply run the built executable:

```bash
./build/desktop/Release/sandbox
```

For Debug builds:

```bash
./build/desktop/Debug/sandbox
```

#### WebAssembly

Use `emrun` to serve the WASM build locally. The `--hostname` and `--serve_after_close` flags are important for proper
isolation and server cleanup:

```bash
# From the project root:
emrun ./build/wasm/Release/sandbox.html --hostname localhost --serve_after_close
```

This launches a local HTTP server at `http://localhost:8080`, and serves the game. The `--serve_after_close` flag
ensures the server continues running after the browser window is closed, which is useful for debugging. To stop the
server, press `Ctrl+C` in the terminal.

For Debug builds:

```bash
emrun ./build/wasm/Debug/sandbox.html --hostname localhost --serve_after_close
```

## Documentation

Start with the **framework reference** in the `docs/` folder:

| Document                                                   | Purpose                                               |
|------------------------------------------------------------|-------------------------------------------------------|
| **[docs/README.md](docs/README.md)**                       | Overview and module map — start here                  |
| **[docs/architecture.md](docs/architecture.md)**           | `EngineContext`, frame loop, phases, pause model      |
| **[docs/ecs-components.md](docs/ecs-components.md)**       | Core ECS patterns, components, singletons, tags       |
| **[docs/scenes.md](docs/scenes.md)**                       | Scene lifecycle, registration, deferred-write hazard  |
| **[docs/systems-reference.md](docs/systems-reference.md)** | Every system and observer, by module                  |
| **[docs/scripting-api.md](docs/scripting-api.md)**         | AngelScript integration, component accessors, globals |
| **[docs/ui.md](docs/ui.md)**                               | Data-driven UI components, layout, interaction        |

Each engine module (e.g., `src/engine/physics/`) includes a `README.md` with deep-dive details.

## Project Structure

The engine is organized into **modules** under `src/engine/`, each providing a specific subsystem (physics, rendering,
UI, etc.). For a detailed breakdown of the directory structure, component catalogs, and system organization, see
**[docs/README.md](docs/README.md)**.

Quick reference:

- `src/engine/engine_context.cpp` — bootstraps all modules in dependency order
- `src/engine/app/` — frame loop and startup
- `src/game/` — game-specific scenes and entities
- `assets/` — game content (JSON configs, models, textures, sounds)
- `build/` — build output (generated, git-ignored)
- `tools/` — build and debugging utilities (e.g., Flecs scripting, CI helpers)
- `docs/` — **[framework documentation](docs/README.md)** (start here for architecture details)

## Including Engine Headers in Game Code

Game code (scenes, entities, game-specific systems) can include a single centralized header to access all engine APIs:

```cpp
#include "engine/engine.hpp"  // Includes all engine modules, components, and singletons
```

This header consolidates the engine's public API. The one-way dependency is maintained (engine files never include game
files), making it safe and efficient for prototyping. See `src/engine/engine.hpp` for the definition.

## Key Concepts

### Data-Driven Design

Game content (levels, UI, behavior) lives in JSON files, not C++ code. Adding a new weapon, level, or NPC means writing
JSON, not branching C++.

### ECS Architecture

- **Components** are plain data structs (no logic)
- **Systems** are stateless functions that query components
- **Singletons** (via `world.get<T>()`) hold global state (assets, input, RNG)
- **Tags** are zero-size structs for filtering (e.g., scene membership, pausability)
- **Observers** respond to component additions/removals without polling

### Modular Subsystems

Each engine feature (physics, rendering, audio, UI) is a **Flecs module**—a self-contained set of components, systems,
and singletons. Import order is fixed (`src/engine/engine_context.cpp`) to ensure dependencies are initialized in the
right order.

### Phases

The frame is split into six phases:

1. **PreUpdate** — poll inputs, lay out UI
2. **OnUpdate** — advance simulation, physics, timers, scripts, debug UI panels
3. **OnStore** — render 3D
4. **OnUI** — render 2D overlays
5. **OnDebugUI** — render the ImGui debug UI on top (see `src/engine/debug/README.md`)
6. **PostUpdate** — clear per-frame state

Systems specify their phase with `.kind(phase)`. This ensures deterministic, lag-free interaction (input is sampled
before interaction systems run, before PostUpdate clears it).

### Pausing

Pausing is **data**, not a branch in every system. Systems that should pause carry the `Pausable` tag. The scene module
toggles pause by switching the active pipeline (one that includes `Pausable` systems, one that doesn't).

### Platform Abstraction

All Raylib calls go through the `platform::Platform` interface. Game code never calls Raylib directly. This makes WASM
and desktop backends interchangeable.

### Transform and WorldTransform

The engine has a strict separation between **authored** and **computed** transforms:

- **Author `Transform`** — this holds local position, rotation, and scale. Always set this when creating entities.
- **Never hand-write `WorldTransform`** — this is world-space data computed from `Transform` by the engine. The
  `TransformSeedWorldTransform` observer seeds it once when an entity is created, then `TransformPropagation` (for
  parented entities) or the physics backend (for dynamic bodies) keeps it in sync.

```cpp
// ✓ Correct: author Transform
e.set<ecs::Transform>({.position = {1.0, 0.0, 0.0}});

// ✗ Wrong: never hand-write WorldTransform after initialization
e.set<ecs::WorldTransform>({/* ... */});  // Will be overwritten by propagation/physics
```

This prevents out-of-phase sync bugs (e.g., a stale cached camera matrix or a physics body left stranded after a
transform change). See `docs/architecture.md` for details.

### Entity Relationships and Linkage

First-class entity relationships allow data-driven composition without hardcoding dependencies:

- **`RenderWith(renderable → material)`** — a renderable links to its material entity via this relationship, which can
  be authored as a nested child with `"link": "render_with"` in JSON.
- **`LookAt(camera → target)`** — a camera aims at a target entity via this relationship, which can be authored with
  `"refs": { "look_at": "TargetName" }` in JSON.

See `docs/architecture.md` and the module READMEs (`render/`, `level/`) for examples.

## Building from Scratch

If you're starting a new game:

1. **Read the framework docs** — start with `docs/README.md` and `docs/architecture.md`
2. **Familiarize with the ECS** — study `docs/ecs-components.md` and existing module READMEs
3. **Add your game module** — create `src/game/your_scene.{hpp,cpp}`, register scenes, define components
4. **Use the scripting API** — wire up AngelScript bindings for fast iteration (`docs/scripting-api.md`)
5. **Define your data** — game content goes in JSON configs under `assets/`

## Scene Equivalence Test

The `tests/scene_equivalence/` target verifies that a scene authored in JSON, scripting, and C++ all produce **identical
ECS component values**. It is the primary regression guard for the engine — run it whenever you add a new
module or change a component's structure.

### Build and Run

```powershell
# Build (desktop-only, part of the main cmake --build):
cmake --preset desktop
cmake --build --preset desktop-release --target scene_equivalence_test

# Run from the project root (output lands in scene_test_output/):
.\build\desktop\tests\scene_equivalence\Release\scene_equivalence_test.exe scene_test_output
```

Passing output lands in `scene_test_output/`:

| File              | Contents                                  |
|-------------------|-------------------------------------------|
| `world_json.json` | Serialized scene (JSON path)              |
| `world_as.json`   | Serialized scene (AngelScript path)       |
| `world_cpp.json`  | Serialized scene (C++ path)               |
| `diff_table.txt`  | 4-column property diff: JSON \| AS \| C++ |

The test exits 0 when all properties match across all three paths, and 1 on any mismatch.

### Committing Results

The output files in `scene_test_output/` are committed to the repository. Running the test after changes and committing
the updated outputs makes regressions visible in code review — a diff that unexpectedly changes
`scene_test_output/diff_table.txt` is a signal that component authoring parity was broken.

### Coverage Catalog

See **[tests/scene_equivalence/COMPONENT_COVERAGE.md](tests/scene_equivalence/COMPONENT_COVERAGE.md)** for the full
component coverage table, known bugs, and the adding-a-new-module checklist.

## Testing Workflows Locally

GitHub Actions workflows can be tested locally using **`act`**, which simulates the CI/CD environment on your machine.

### Installing `act`

Visit the [official installation guide](https://nektosact.com/installation/index.html) for your OS. Quick reference:

**macOS (Homebrew):**

```bash
brew install act
```

**Windows (Chocolatey):**

```bash
choco install act-cli
```

**Linux (from binary):**

```bash
curl --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash
```

### Prerequisites for `act`

- **Docker** must be installed and running (acts uses containers to simulate runners)
- Run `docker --version` to verify installation

### Testing the Build Workflow

Test the main build workflow that builds desktop (Windows/Linux/macOS) and WebAssembly:

```bash
# Test on your current OS (simulates that OS's runner)
act push -j build

# Dry-run to see what would execute
act push -j build --dry-run

# Test a specific matrix job (e.g., Linux desktop)
act push -j build --matrix os:ubuntu-latest --matrix target:"Desktop (Linux)"

# Test WASM build specifically
act push -j build --matrix os:ubuntu-latest --matrix target:"WebAssembly"
```

Multiple matrix jobs can be tested by specifying the `--matrix` flag multiple times.

### Testing the Deploy Workflow

Test the GitHub Pages deployment workflow (WASM build + upload):

```bash
# Test deploy to Pages (simulates main branch push)
act push -b main -j build-and-deploy

# Dry-run to see what would execute
act push -b main -j build-and-deploy --dry-run
```

### Important Notes

- **Docker required** — `act` runs jobs inside containers
- **Slow first run** — Initial builds take longer (dependencies compile)
- **No artifact upload** — Artifacts build locally but don't upload to GitHub
- **Resource-intensive** — VCPKG dependencies take time to build; allocate 30+ minutes for full build
- **Cache** — `act` doesn't persist Docker layer caches between runs, but subsequent builds within the same run are
  faster

### Debugging `act`

If a workflow fails:

```bash
# Verbose output
act push -j build -v

# Use specific Docker image (useful if default doesn't match your setup)
act push -j build -P ubuntu-latest=ghcr.io/catthehacker/ubuntu:full-latest

# Keep Docker container after run (inspect logs manually)
act push -j build --reuse
```

## Debugging

### Flecs Remote API

The Flecs REST API (on desktop only; not supported in browsers) runs at `localhost:27750` and lets you inspect the live
ECS state:

- **Explorer**: https://flecs.dev/explorer — query entities, components, systems in real-time
- **Script helper**: `tools/flecs-api.js` — Node.js CLI for switching scenes, listing entities, etc.

Example:

```bash
# List all scenes and their active state
node tools/flecs-api.js list-scenes

# Switch to the "Game" scene
node tools/flecs-api.js switch-scene Game
```

### Debug Drawing

Held-down keys trigger debug features (check `src/engine/input/` for which keys). Physics debug rendering, entity
outlines, and profiler stats are all behind debug-only systems.

## Profiling

The project ships two complementary profiling tools: a **live Tracy integration** for frame-accurate timeline
tracing, and a **JSONL telemetry log** that records per-system timing trends over time for analysis in a browser.

### Profiling Build

A dedicated preset enables both tools simultaneously with `RelWithDebInfo` (optimised timings, full stack traces):

```bash
cmake --preset desktop-profile
cmake --build --preset desktop-profile-relwithdebinfo
```

What the profiling preset enables:

- **`ENABLE_TRACY=ON`** — Flecs OS API hooks feed every system/observer into Tracy zones automatically; explicit
  zones wrap `BeginFrame`, `EndFrame`, `BeginMode3D`, `EndMode3D`, and `SubmitBatch`; draw-call and instance counts
  are emitted as live `TracyPlot` graphs.
- **`ENABLE_PROFILE_LOGGING=ON`** — `stats-telemetry.jsonl` is created and written from frame 1 (no in-game
  toggle needed).

Both options can also be used independently with the normal desktop build by passing `-DENABLE_TRACY=ON` or
`-DENABLE_PROFILE_LOGGING=ON` to CMake.

### Tracy Frame Profiler

[Tracy](https://github.com/wolfpld/tracy) is a real-time frame profiler. It shows the exact timeline of every Flecs
system and observer, plus the outer render calls, so you can spot hitches at frame granularity.

#### Setup

1. Download or build the **Tracy capture app** from the
   [Tracy releases page](https://github.com/wolfpld/tracy/releases). The profiling build connects to it over TCP.
2. Start the Tracy capture app — it listens on port **8086** by default.
3. Launch the profiling build of the game. Tracy connects automatically on startup.

> **Note:** The Flecs perf-trace hooks (`perf_trace_push_`/`pop_`) require Flecs to be compiled with
> `FLECS_PERF_TRACE` defined. The `desktop-profile` preset handles this automatically via the custom vcpkg triplet
> `triplets/x64-windows-perf.cmake`, which adds `-DFLECS_PERF_TRACE` to the compiler flags for all vcpkg packages.
> The first configure with this preset rebuilds vcpkg packages into a separate ABI slot (`x64-windows-perf`), so
> standard `desktop` builds are unaffected.

#### What you get

| Zone                          | Description                                              |
|-------------------------------|----------------------------------------------------------|
| All Flecs systems + observers | Automatic via `perf_trace_push_`/`pop_` OS API hooks     |
| `Platform::BeginFrame`        | Frame setup + clear (including any vsync stall)          |
| `Platform::EndFrame`          | GPU present — the dominant frame-time contributor        |
| `Platform::BeginMode3D`       | Camera + shader setup                                    |
| `Platform::EndMode3D`         | Closing the 3D pass                                      |
| `Platform::SubmitBatch`       | Batch grouping + instanced draw calls                    |
| `draw_calls` plot             | Live draw-call count per frame (graph in Tracy timeline) |
| `instances` plot              | Live instanced-primitive count per frame                 |

`FrameMark` is emitted after `PostTick()` so Tracy's frame boundaries include deferred tasks.

### System Stats Overlay (In-Game)

Press **F4** in any scene to toggle the stats panel. It shows:

- FPS, render draw calls, instance count, and batch CPU times
- Per-module rollup of Flecs system timings, with rolling average and spike/variability colour-coding (green = steady,
  orange = variable, red = spiking)
- Per-system matched entity count alongside the timing, so expensive-per-entity systems stand out

The **"Record telemetry"** checkbox at the top of the panel writes `stats-telemetry.jsonl` to the working directory (the
build output folder). The profiling preset starts recording automatically from frame 1.

### Stats Viewer

`tools/stats-viewer.html` is a self-contained browser viewer for the JSONL telemetry. Open it directly from disk
to review a past run, or serve the build output folder over HTTP for live auto-refresh.

#### Past-run review (no server needed)

Open `tools/stats-viewer.html` in a browser and use the **File** picker to load `stats-telemetry.jsonl`.

#### Live refresh during a profiling session

The JSONL file lives in the build output folder. Serve that folder over HTTP, then point the viewer at it:

```powershell
# PowerShell — serve the profiling build output
cd build/desktop-profile/RelWithDebInfo
npx http-server -p 8000 --cors
```

Then open `tools/stats-viewer.html`, paste `http://localhost:8000/stats-telemetry.jsonl` into the URL field, and
tick **auto-refresh 1s**.

> **Node.js required** for `npx http-server`. Alternatively: `python -m http.server 8000` from the same folder
> (no CORS header, so only works for same-origin — use `npx http-server` for the auto-refresh URL mode).

#### What the viewer shows

| Panel                  | Description                                                                                 |
|------------------------|---------------------------------------------------------------------------------------------|
| **ms over time**       | Rolling per-system timing; click ⤢ to expand full-screen                                    |
| **Entities over time** | Matched entity count per system — compare with the ms chart to see if cost scales with work |
| **Hotspots table**     | Sortable; includes **ms / 1k entities** (per-entity cost — the key scaling metric)          |
| **Drill-down**         | Click any table row → side-by-side ms + entities time series for that system                |

Use the sidebar to select which systems are plotted; **Top 8** picks the 8 highest-max-ms systems.

---

## Dependencies

All dependencies are managed by VCPKG and specified in `vcpkg.json`:

- **raylib** — Platform abstraction, rendering, windowing
- **flecs** — Entity-Component-System framework
- **joltphysics** — Physics simulation (desktop; omitted for WASM due to size)
- **glm** — Math library
- **stb** — Image/font utilities
- **imgui** — Debug UI toolkit
- **nlohmann_json** — JSON parsing & serialization
- **spdlog** — Logging
- **angelscript** — Scripting language

## Troubleshooting

### CMake can't find toolchain

Make sure `VCPKG_ROOT` is set:

```bash
echo $VCPKG_ROOT  # macOS/Linux
echo %VCPKG_ROOT%  # Windows
```

For WASM, also check `EMSDK`:

```bash
echo $EMSDK  # macOS/Linux
echo %EMSDK%  # Windows
```

### Build fails with missing dependencies

VCPKG downloads and builds dependencies on first configure. If a build stalls or fails:

1. Delete `build/` and try again
2. Check that `VCPKG_ROOT` points to the right directory
3. For WASM, ensure `EMSDK` is active: `source $EMSDK/emsdk_env.sh` (or `. %EMSDK%/emsdk_env.ps1` on Windows)

### Emrun won't start

- Make sure EMSDK is in your PATH: `which emrun` (macOS/Linux) or `where emrun` (Windows)
- Use the full path if needed: `/path/to/emsdk/upstream/emscripten/emrun`

---

**Last updated**: October 2026 **Framework version**: 0.6.0
