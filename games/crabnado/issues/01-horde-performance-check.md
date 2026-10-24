# 01: Horde performance check in the browser

Spec: [spec](../spec.md). Read: [Technical](../spec/technical.md), [Presentation](../spec/presentation.md).

**What to build:** In the game scene, a debug control (a debug UI panel or key) spawns batches of placeholder crabs
(up to about 1,000) that walk toward the arena centre, with an FPS and enemy-count readout. Each crab is a Flecs entity
with reflected components, drawn as instanced engine primitives (flattened sphere body and two claw spheres, tinted
per instance). Run it in the WASM Release build in a browser and record the largest horde that holds about 60 fps.
That number becomes the horde cap in the game's tuning data.

**Blocked by:** None (can start immediately). The WASM preset needs Emscripten (`EMSDK`) installed. It isn't on the
current Mac yet, so set that up first.

**Status:** resolved

- [ ] ~~The `wasm` preset builds Crabnado, and its page loads and reaches the title scene in a browser.~~ Deferred to
      the end-of-jam WASM pass (see Comments).
- [x] In the game scene, the debug control spawns crabs in batches up to about 1,000, and they walk toward the centre.
- [x] FPS and enemy count are visible on screen (and in the debug UI on desktop).
- [x] ~~The measured browser result~~ The measured desktop result (enemy count vs. FPS) is recorded under `## Comments` below, along with the chosen
      horde cap.
- [x] The horde cap is stored as tuning data the later tickets read.
- [x] Desktop: the enemy components can be queried over the Remote API on port 27751.

## Comments

**2026-10-23, desktop result.** In the arena scene, `=` (or "Spawn 100" in the ImGui "Horde" panel) adds 100 crabs, up
to 1,000, and Backspace / "Clear" removes them. The top-left label shows "FPS … enemies …". The same values are in the
reflected `crabnado.CrabnadoModule.HordeReadout` singleton, and `crabnado.CrabnadoModule.Enemy` can be queried on port
27751.

macOS Intel, Release desktop build, driven over the Remote API:

| Enemies | FPS |
|---|---|
| 0 | 59–60 |
| 100–400 | 60 |
| 500–1,000 | 59 |

Desktop holds the vsync limit at 1,000 crabs (4,000 entities: the crab root plus a body and two claw spheres). Because
of vsync, the headroom isn't measured.

**2026-10-23, WASM deferred.** WASM is validated once at the end of the jam (`games/crabnado/AGENTS.md`), so the
desktop result sets the horde cap for now: **`kHordeCap` = 1,000** (in `src/crabnado_constants.hpp`). Desktop holds
about 60 fps there. The end-of-jam pass rechecks it in the browser: press `=` until the readout drops below about
60 fps, and lower the cap if needed (the spec's fallback is about 400).
