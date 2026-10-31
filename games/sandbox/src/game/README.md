# Sandbox: where to find a working example

`src/game/` is the `sandbox` executable: engine demo scenes, not a game. None of them is a minimal template; each is
the working reference for a few engine features. Find the feature you need below, read that file, and copy the pattern.

| Need | Look at |
| --- | --- |
| A scene's shape: pipeline, `Load`/`Unload` under one root entity, `Tick` | `synth_scene.cpp` (smallest), `cube_scene.cpp` |
| Scene content from a Level JSON (camera + `look_at`, lights, primitives, materials, `render_with` children, scripts) | `cube_scene.cpp` + `assets/levels/demo.level.json` |
| Game-registered level loaders and tags | `tilemap_scene.cpp` (`RegisterLoaders`, `bt_wanderer`) |
| A dialog tree built from script and started from a tile's `onenter` | `assets/scripts/tilemap_dialog.as`, `src/engine/dialog/dialog_module.cpp` (`RegisterScripting`) |
| Thousands of tinted primitives that stay one instanced draw call (inline `Material`, not `RenderWith`) | `src/engine/render/README.md` ("Many entities"), `games/crabnado/src/horde.cpp` |
| A continuous particle emitter (`particle_emitter` in JSON) | `assets/levels/demo.level.json`, `src/engine/particles/README.md` |
| Timers, cooldowns and tweens (a pulse, a fade) | `assets/scripts/timer_tween_demo.as`, `src/engine/timer/README.md` |
| Building a `GridNavGraph` and sending path requests | `tilemap_scene.cpp` (`SetupPathfindingAgent`), `src/engine/pathfinding/README.md` |
| A 3D debug overlay: project world points to screen and draw lines in `OnUI`, toggled by F3 | `PathDebugDrawSystem` in `src/engine/pathfinding/pathfinding_module.cpp` |
| An ImGui panel | `EngineDebugPanel` in `src/engine/debug/debug_ui_module.cpp` |
| Retained 2D UI (labels, buttons, sliders, prompts, pause modal) | `synth_scene.cpp`, `cube_scene.cpp` (`BuildUI`) |
| Edge-triggered keys in a scene | `synth_scene.cpp` (`Tick`: `WasKeyPressed`) |
| Script-facing functions and callbacks | `src/engine/dialog/dialog_module.cpp` (`RegisterScripting`), `assets/scripts/tile_callback_demo.as`, `assets/scripts/tilemap_dialog.as` |
| Save data | `meta_save_example.cpp`, `save_ui.cpp` |

## Gaps (no example yet)

- **One-shot particle bursts.** `ParticleEmitter` is rate-based. A burst (e.g. a puff when something lands) needs an
  emitter turned on for a frame or a short lifetime; there is no burst API.
- **Relationship-driven gameplay state** (one entity *carries* or *holds* another). The engine
  relationships (`RenderWith`, `LookAt`, `UseRegistry`) show the declaration and loader side only.
