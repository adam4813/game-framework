# Spec 1 · Debugging & tuning

Part of [Signal Room spec 1: Room & bootstrap](../spec.md). The Room panel, debug overlays, Explorer-visible components and the platform rule.

## User stories

54. As a developer, I want an ImGui Room panel listing the Shift files with a Reload button, so that I can edit Layout
    or Shift JSON and see it without restarting.
55. As a developer, I want movement tuning sliders in the panel (walk speed, ramp up/down, turn time, dash multiplier,
    duration and cooldown) that apply live, so that I can tune feel in play.
56. As a developer, I want debug overlays for the floor grid, solid cells, worked sides, Operator circle and the
    targeting ray and cone, so that I can see why a Target was or wasn't picked.
57. As a developer, I want the Operator's position, velocity, facing, carried Item and Target visible as reflected
    components in the Flecs Explorer, so that I can inspect them at runtime.
58. As a developer, I want a headless test harness that loads a Shift, presses keys, steps frames and queries the
    world, so that room behaviour is covered without a window.

## Implementation decisions

- **Debug UI:** a Room panel (Shift list + Reload, tuning sliders, overlay toggles) built on the platform `DebugUi`
  interface, scoped to the Room scene and running on the main thread. Debug draw goes through `Platform`.
- **Platform:** no direct Raylib calls from game code. Input comes through the engine `InputState`, drawing goes through
  render components and `Platform`.
