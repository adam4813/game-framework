# 01: Horde performance check in the browser

Spec: [spec](../spec.md). Read: [Technical](../spec/technical.md), [Presentation](../spec/presentation.md).

**What to build:** In the game scene, a debug control (a debug UI panel or key) spawns batches of placeholder crabs
(up to about 1,000) that walk toward the arena centre, with an FPS and enemy-count readout. Each crab is a Flecs entity
with reflected components, drawn as instanced engine primitives (flattened sphere body and two claw spheres, tinted
per instance). Run it in the WASM Release build in a browser and record the largest horde that holds about 60 fps.
That number becomes the horde cap in the game's tuning data.

**Blocked by:** None (can start immediately). The WASM preset needs Emscripten (`EMSDK`) installed. It isn't on the
current Mac yet, so set that up first.

**Status:** ready-for-agent

- [ ] The `wasm` preset builds Crabnado, and its page loads and reaches the title scene in a browser.
- [ ] In the game scene, the debug control spawns crabs in batches up to about 1,000, and they walk toward the centre.
- [ ] FPS and enemy count are visible on screen (and in the debug UI on desktop).
- [ ] The measured browser result (enemy count vs. FPS) is recorded under `## Comments` below, along with the chosen
      horde cap.
- [ ] The horde cap is stored as tuning data the later tickets read.
- [ ] Desktop: the enemy components can be queried over the Remote API on port 27751.
