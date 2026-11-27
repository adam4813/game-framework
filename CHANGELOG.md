# Changelog

Framework releases, newest first. Each entry opens with the version's highlight, then lists what changed by area.
Releases up to 0.10.0 were back-filled from the git tags, so they hold highlights rather than every change. Game Targets carry their own `project()` version, and their
releases are noted under the Framework release they shipped with.

## 0.12.0 - 2026-11-26

**Highlight: any number of point lights through clustered forward shading, with two new Game Targets, Farm and Signal
Room.**

### Added

- **Clustered forward point lights** (#136, ADR 0009): a scene can author any number of `PointLight`s. Each Frame the
  platform culls them to the camera, keeps the most important within a budget (512 visible, 64 per cluster) and lists
  them per cluster for `lighting.glsl`. `PointLight` gains `range`, derived from the attenuation terms when unset.
  Light counts, drops and cluster build time show in `RenderStats`, the profiler and the stats panel. The sandbox's
  Many Lights scene demonstrates it.
- **Script constants for reflected enums** (#128): `RegisterEnumConstantsForScripts` exposes an enum to scripts as
  `<Prefix>_<Name>` ints. `Projection_*`, `Motion_*`, `PathFollowMode_*`, `TextAlign_*` and `StackDirection_*` are
  registered, and `RigidBody`, `PathFollow`, `Label` and `Stack` reflect those members.
- **`ENABLE_REST_API` CMake option** (#141): `OFF` compiles the Flecs REST server out, and `InitializeRemoteAPI`
  becomes a logged no-op. CI's game artifacts build with it off, so a shipped game opens no debug port.
- **READMEs for the app and scene modules** (#129).
- **Farm 0.1.0** (#142): a farm life-sim on a deterministic procedural island (terrain, rivers, forests, villages and
  roads), with crops, NPC dialog and shops, game time, inventory and multi-slot saves.
- **Signal Room 0.1.0** (#143): the Room and its core loop: Operator movement, Items, Tickets and the Window, Stations,
  Radios and the Antenna Coupler, scoring and the Handler Chain, the HUD, and the Briefing card, results and Retry.

### Changed

- **The web build needs WebGL 2** (#136): an overlay triplet builds raylib for OpenGL ES 3, and the page shows a
  message in a browser without it.
- **One shader source for every target** (#136): shaders live in `assets/shaders/`, and the backend prepends the
  `#version` line and expands `#include`. A game shader in `glsl330/` or `glsl100/` moves up one folder and drops its
  `#version` line.
- **`Platform::SetLighting` takes the point lights as a span** and returns `LightingStats` (#136). `LightParams` no longer
  holds them.
- **`MotionType`, `PathFollowMode`, `TextAlign` and `StackDirection` lose their `uint8_t` underlying type** (#128), so
  the serializer reports real values.
- **Loaded meshes are instanced and frustum-culled** (#135): static opaque models batch into one instanced draw per
  sub-mesh and are culled by a bounding sphere from `Platform::MeshBounds`, which `FrustumCullRadius` overrides. A
  culled model casts no shadow.
- **Faster primitive enqueue** (#140): up to 10x less enqueue time in a world with many small tables.

### Fixed

- **An unknown camera `projection` warns** (#128), naming the value, the entity and the accepted names, then falls back
  to Perspective.
- **`lit_inst.vs` applies vertex colors** and keeps normals right under non-uniform scale (#135).

## 0.11.0 - 2026-11-22

**Highlight: a UI framework and event-driven scripting, with Crabnado 1.0 as the first game built on them.**

### Added

- **UI framework** (#111): `Image` widget with fit modes, atlas sub-rects and tint; `UIAnchor` anchored positioning
  resolved in `UILayout`; the `UICapture` singleton, so UI-owned pointer and keyboard input is withheld from gameplay.
- **`EventHost` for event-driven scripts** (#123): a named event table, ordered script slots, handlers checked at load,
  game commands queued from handlers and applied at a safe point, and `After()` timers. A throwing handler disables
  only its own script.
- **Script function calls** (#120, #123): `IScriptInstance` can list a script's functions and call one by name.
- **Crabnado 1.0.0** (#120): a three-Act campaign driven from `campaign.json` and `bosses.json`, bosses, and the wave
  ride, wind-up and charge-back enemy behavior.
- **Game Target versions**: every Game Target now declares `project(<name> VERSION ...)`, which `setup_game_target`
  turns into the `GAME_VERSION` define.

### Changed

- **Framework logging goes through `ecs::Log`** (#125, renamed from `WorldLog`); scripting, physics and level load
  were moved onto it.
- **Sandbox assets moved** (#126): the sandbox's levels, scripts, models, textures, AI and tilemap data now live in
  `games/sandbox/assets/`. The root `assets/` keeps only shaders, `config/profiling.json` and `audio/sfx/click.wav`.
  A game that read the old shared tileset or atlas needs its own copy.

### Fixed

- **A throwing script is disabled** (#122), instead of logging every frame. `self` in a script is its host entity, and
  `GetT`/`MutT` on a missing component warns once per entity and component.

## 0.10.0 - 2026-11-17

**Highlight: a translucent render pass and depth-tested 3D debug lines.**

- Render: a translucent pass for models and primitives (#100) and depth-tested 3D line drawing (#94).
- Physics: raycasts and body wireframes draw as 3D lines (#102). A change to an existing `RigidBody` or
  `CollisionShape` now applies to the live body.
- Level: a public JSON file-reference resolver and file-backed headless saves (#97).
- CI: a workflow cleans up PR caches so `main`'s longer-lived caches survive.

## 0.9.0 - 2026-11-15

**Highlight: physics raycasts.**

- Raycast queries with collision layers and character inner bodies (#87).
- A character's state restores for replay in one call (#88).

## 0.8.1 - 2026-11-14

- Docs: ADR-0006 brought up to date with Game Target discovery (#62).

## 0.8.0 - 2026-11-14

**Highlight: Game Targets, with Crabnado and Football as the first games beside the sandbox.**

- Build: every `games/<name>/CMakeLists.txt` is discovered by glob and the sandbox became a Game Target (#12). A game's
  own `assets/` layers over the shared root assets (#14).
- Games: the Crabnado game jam version landed (#26, tagged `crabnado-jam`), then the Football roguelike deckbuilder
  prototype (#55).
- Physics: static mesh colliders built from a model file (#15), a character body on Jolt `CharacterVirtual` (#16), two
  physics worlds with a `HeadlessPlatform` (#42), and replaying a single character for one tick (#52). Primitive
  colliders sit at their authored offset (#23).
- Render and animation: skeletal animation with GPU skinning (#20), point lights in the lit shaders (#19) and
  double-sided materials (#24).
- Engine: a fixed-step simulation (#51), a named logger per world (#48), cursor lock with input polled in `OnLoad`
  (#18), `InitializeRemoteAPI` that can ignore `FLECS_REST_PORT` (#43), and an owned handle for a path-less asset (#25).
- CI and tooling: vcpkg ports are cached and only changed targets build (#41), the backlog moved to GitHub issues (#38)
  and the vcpkg baseline was bumped (#54).

## 0.7.0 - 2026-10-27

**Highlight: CI builds every game, and Intel macOS gets presets.**

- CI builds every game under `games/` for Linux, Windows and WASM (#8).
- `macos-intel` and `macos-intel-test` presets set the MacPorts LLVM 20 toolchain for the C++23 engine (#9).

## 0.6.0 - 2026-10-23

**Highlight: orthographic camera and headless tooling.**

- Camera: an orthographic projection mode (#7).
- Headless: `NullPlatform` takes an asset directory so headless loads resolve real assets.
- Tooling: the Flecs API and MCP tools resolve a component's leaf name to its dot-path and can wait for the REST
  server; `tools/screenshot.ps1` captures a desktop window or a WASM build.

## 0.5.0 - 2026-10-17

**Highlight: CI runs the unit tests on every PR.**

- CI adds the GoogleTest step (`desktop-test` and `ctest`) to the build workflow (#6).
- Agent rules for `Load()` initialization order and post-merge test-count verification.

## 0.4.0 - 2026-10-14

**Highlight: GoogleTest integration and the project's domain docs.**

- GoogleTest set up for unit tests (#5), plus a glossary and five ADRs.
- Build presets, and a fix to the sandbox WASM deploy script. A script UI button is parented correctly.

## 0.3.0 - 2026-10-11

**Highlight: the module set grows: noise, dialog, AI, pathfinding, profiling and UI widgets.**

- New modules: noise generation and sampling, dialog, graph, AI and pathfinding, and profiling with a Tracy connection
  plus a stats panel and viewer (#3).
- UI is split into smaller files by scope and gains more widgets.
- Level: JSON levels load from a string, and a tilemap serializer was added.
- Tests: a scene equivalence test, and a natvis helper for the debugger.

## 0.2.0 - 2026-09-13

**Highlight: most modules refactored to idiomatic declarative Flecs patterns.**

- Level and script demos reworked to match, with expanded `demo.level.json` and a tilemap player level.

## 0.1.1 - 2026-09-07

- Core: `LoadJsonDirectory` for JSON utilities, and probability and weighted-pick helpers on `RngState`.

## 0.1.0 - 2026-08-18

**Highlight: the initial project: Raylib, Flecs and CMake with vcpkg.**
