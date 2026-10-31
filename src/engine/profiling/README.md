# Profiling Module

Centralizes runtime instrumentation behind a backend-agnostic interface. Subsystems emit **plots**
(named scalar time-series), **zones** (scoped timers), and **frame marks** through `profiling::*`
calls that carry no knowledge of the backend — whether data is sent to Tracy, a file, or nowhere is
an implementation detail hidden in `profiling_trace.cpp`. No `#ifdef`s, macros, or Tracy headers leak
to callers; in a non-profiled build every call compiles to a no-op.

Two emission paths exist:

- **Queued** — a system writes per-frame aggregates into a `ProfileStats` component; `ProfilerFlush`
  drains every `ProfileStats` once per frame (after `PostFrame`) and emits them. Use `RegisterStatsEmitter`.
- **Immediate** — `profiling::Plot(...)` emits a scalar right away (thread-safe), for worker-thread
  code that cannot queue into a component.

## Structure

| File                        | Purpose                                                                      |
|-----------------------------|------------------------------------------------------------------------------|
| `profiling_trace.hpp/.cpp`  | Consumer API: `Zone`, `Plot`, `MarkFrame`, module filter. Hides the backend. |
| `profiling_components.hpp`  | `PlotEntry`, `ProfileStats` component, `ProfilerConfig` singleton.           |
| `profiling_module.hpp/.cpp` | `ProfilerModule`, the flush phase/systems, and `RegisterStatsEmitter`.       |
| `profiling.hpp`             | Public umbrella header.                                                      |

## Consumer API (`profiling_trace.hpp`)

```cpp
// Scoped timer — call site captured automatically via std::source_location. No macro needed.
void Solve() {
    profiling::Zone _zone{"Pathfinder::Solve"};
    ...
}

// Immediate scalar plot (thread-safe). `module_key` is checked against the filter.
profiling::Plot("PathfindingModule", "astar.nodes_expanded", static_cast<int64_t>(n));

// Frame boundary (called once per frame by main).
profiling::MarkFrame();

// Zone around every Flecs system (called once in main, after ecs_os_set_api_defaults()
// and before the world is created). No-op unless Tracy is enabled.
profiling::InstallFlecsSystemZones();
```

## Components

### `ProfileStats`

Attach to an entity to route its metrics through the queued pipeline. Systems push entries each
frame; `ProfilerFlush` drains and clears them. Usually created for you by `RegisterStatsEmitter`.

```cpp
struct ProfileStats {
	std::vector<PlotEntry> entries;
	void Plot(std::string_view module_key, std::string_view name, int64_t /* or double/float */ value);
};
```

`module_key` (e.g. `"RenderModule"`, `"PathfindingModule"`, `"World"`) gates Tracy output via the
per-module filter. `name` is the plot key (e.g. `"render.fps"`).

### `ProfilerConfig` (Singleton, optional)

Opt-in JSON-lines file output. Absent by default. An `OnSet` observer opens the stream so it is ready
before the (readonly) flush runs.

```cpp
world.set<engine::profiling::ProfilerConfig>({.file_enabled = true, .file_path = "profiler.jsonl"});
```

## Emitting per-frame stats — `RegisterStatsEmitter`

Registers a `PostFrame` system that owns a dedicated `ProfileStats` entity and invokes your callback
each frame, removing the entity/plumbing boilerplate. Returns the system entity so you can scope it.

```cpp
profiling::RegisterStatsEmitter(world, "RenderStatsPlot",
    [](const flecs::world& w, profiling::ProfileStats& stats) {
        const auto& rs = w.get<render::RenderStats>();
        stats.Plot("RenderModule", "render.fps", static_cast<int64_t>(rs.fps));
    }).add<scene::GameScene>();   // optional: scope to a scene
```

## Systems

| System / Observer   | Phase                | Role                                                                 |
|---------------------|----------------------|----------------------------------------------------------------------|
| `WorldStatsEmit`    | `PostFrame`          | Pushes `world.entities` / `world.tables` (via RegisterStatsEmitter). |
| `<name>` emitters   | `PostFrame`          | One per `RegisterStatsEmitter` call (render, pathfinding, …).        |
| `ProfilerFlush`     | `ProfilerFlushPhase` | Drains every `ProfileStats`: emits to backend + file, clears.        |
| `ProfilerConfigSet` | Observer (`OnSet`)   | Opens the output file when `ProfilerConfig` is set.                  |

`ProfilerFlushPhase` depends on `PostFrame`, so every producer for the frame is flushed together.

## Per-module filter — `assets/config/profiling.json`

Disable plotting for specific modules without code changes:

```json
{"disabled_modules": ["PathfindingModule", "RenderModule"]}
```

Loaded once at startup (`EngineContext` calls `profiling::LoadModuleFilter`); every emit consults
`profiling::IsModuleEnabled(module_key)`. An empty list enables all modules.

## Integration

Imported as a required module in `EngineContext` (after render/ui/timer):

```cpp
world_.import<engine::profiling::ProfilerModule>();
```

Flecs system-level timeline instrumentation (bracketing each system with a zone) is wired into the
Flecs OS API by `profiling::InstallFlecsSystemZones()`, which each Game Target's `main()` calls at
process start. Build-side, `setup_game_target` calls `setup_tracy` (`-DENABLE_TRACY=ON`) for every
Game Target, and `-DENABLE_PROFILE_LOGGING=ON` auto-starts the JSONL telemetry in all of them.
