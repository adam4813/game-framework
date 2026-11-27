# Engine-Owned Backends Behind Per-Concern Seams

_Amended 2026-11-26. It was "Platform Abstraction via the `Platform` Interface": one interface for windowing, input,
audio, saves and drawing. ADR 0010 moved drawing behind the `Renderer` and ADR 0012 moved resources into Assets, which
left `Platform` a grab bag of unrelated concerns, so it is split and the class goes._

OS and library calls live behind backend interfaces inside the Module that serves each concern: the window (`window/`),
audio (`audio/`), the debug UI (`debug/`) and saves (`save/`), each an `I<X>Backend` owned by a world Singleton with
its adapters in `<module>/<library>/`, as scripting owns its backend. Input has no backend: the window's per-Frame poll
fills an `InputSample` that the input Module reads, because every windowing library delivers input through its own
event pump. Drawing and GPU resources go through the `Renderer` (ADR 0010, 0012). The `EngineContext` owns the Window
and the `Renderer` itself, since the device, surface and window must tear down in order, after the world.

Engine and game code reach these through ECS data (`FrameClock`, `Canvas`, `WindowSettings`, `CursorLock`), Flecs
events (`WindowResized`, `FocusChanged`, `CursorLockChanged`, `CloseRequested`) and Module functions
(`audio::PlaySound`, `save::Write`), never through a backend type. raylib, SDL, Dawn, imgui's platform bindings and
`#ifdef __EMSCRIPTEN__` appear only in adapters and in `engine::RunApp`, which picks the adapters for the build target.
Each interface's base class is its null adapter, so a headless world (a server, a tool, the tests) needs no setup.
This keeps the original aim, game code that never sees the library underneath, while letting each concern change its
library on its own.
