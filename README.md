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

The executable is at `build/desktop/Release/game` (or `Debug/game`).

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
./build/desktop/Release/game
```

For Debug builds:

```bash
./build/desktop/Debug/game
```

#### WebAssembly

Use `emrun` to serve the WASM build locally. The `--hostname` and `--serve_after_close` flags are important for proper
isolation and server cleanup:

```bash
# From the project root:
emrun ./build/wasm/Release/game.html --hostname localhost --serve_after_close
```

This launches a local HTTP server at `http://localhost:8080`, and serves the game. The `--serve_after_close` flag
ensures the server continues running after the browser window is closed, which is useful for debugging. To stop the
server, press `Ctrl+C` in the terminal.

For Debug builds:

```bash
emrun ./build/wasm/Debug/game.html --hostname localhost --serve_after_close
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

The frame is split into five phases:

1. **PreUpdate** — poll inputs, lay out UI
2. **OnUpdate** — advance simulation, physics, timers, scripts
3. **OnStore** — render 3D
4. **OnUI** — render 2D overlays
5. **PostUpdate** — clear per-frame state

Systems specify their phase with `.kind(phase)`. This ensures deterministic, lag-free interaction (input is sampled
before interaction systems run, before PostUpdate clears it).

### Pausing

Pausing is **data**, not a branch in every system. Systems that should pause carry the `Pausable` tag. The scene module
toggles pause by switching the active pipeline (one that includes `Pausable` systems, one that doesn't).

### Platform Abstraction

All Raylib calls go through the `platform::Platform` interface. Game code never calls Raylib directly. This makes WASM
and desktop backends interchangeable.

## Building from Scratch

If you're starting a new game:

1. **Read the framework docs** — start with `docs/README.md` and `docs/architecture.md`
2. **Familiarize with the ECS** — study `docs/ecs-components.md` and existing module READMEs
3. **Add your game module** — create `src/game/your_scene.{hpp,cpp}`, register scenes, define components
4. **Use the scripting API** — wire up AngelScript bindings for fast iteration (`docs/scripting-api.md`)
5. **Define your data** — game content goes in JSON configs under `assets/`

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

**Last updated**: September 2026 **Framework version**: 0.1.0
