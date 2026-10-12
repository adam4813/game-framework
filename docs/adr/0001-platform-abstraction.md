# Platform Abstraction via the `Platform` Interface

All rendering, windowing, and input calls are routed through the abstract `platform::Platform` interface rather than calling Raylib directly from engine or game logic. This keeps `#ifdef __EMSCRIPTEN__` and Raylib-specific types out of the rest of the codebase, lets the desktop and WASM targets share one concrete backend, and leaves the door open to swap backends (or add a headless test backend) without touching engine or game code.
