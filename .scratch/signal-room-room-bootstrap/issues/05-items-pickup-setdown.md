# 05: Items: pick up, carry & set down

Spec: [Signal Room spec 1: Room & bootstrap](../spec.md). Read: [Targeting, pick-up & set-down](../spec/targeting-and-items.md), [Camera & greybox reads](../spec/greybox-and-camera.md), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (this game's main: branch off it, PR back into it, never into `main`)

**What to build:** A placeholder Item waits on a Counter in Shift 1. Empty-handed at that Counter, the player presses
the action key to pick it up. The Item is then shown in front of the Operator (cards tilted toward the camera). They
can carry it, dash while carrying, and set it down on any free Counter with a dust puff. Wrong presses are harmless,
and an Item is never on the floor. This ticket closes spec 1: its exit check runs here.

**Blocked by:** 04 (Targeting & the Target brighten).

**Status:** ready-for-agent

- [ ] An Item catalogue (data) gives each Item an `id`, a greybox `shape` key, `carry_speed` (default 1.0) and
      `tilt_to_camera`. Shift 1 uses one placeholder Item.
- [ ] The Shift file gains `counter_items` (Counter placement name → Item id). An unknown Counter or Item id is a load
      error naming it.
- [ ] Interaction runs on the `interact` press event. Empty-handed at a Counter holding an Item → pick up. Carrying at
      an empty Counter → set down. Anything else (an occupied Counter while carrying, an empty Counter while
      empty-handed, an in-service Station) does nothing.
- [ ] Carrying and holding are relationships. Every Item has exactly one holder (an Operator or a Counter), and there
      is no floor state.
- [ ] On interacting, the Operator turns to face the Target and never snaps position.
- [ ] The carried Item's `carry_speed` scales walk and dash.
- [ ] A set-down plays a dust-puff burst from the particles module, with intensity in data.
- [ ] Harness tests:
  - pick up → walk → set down moves the Item to the second Counter;
  - setting down on an occupied Counter refuses, and the Item stays carried;
  - pressing at an in-service Station does nothing;
  - dashing while carrying works;
  - an Item with `carry_speed` 0.5 halves the walk distance;
  - Reload restores the starting `counter_items`.
- [ ] Spec 1 exit (runtime, per `AGENTS.md`): in the desktop build, through the Flecs Remote API, switch to the Room
      scene, then walk, dash and collide through the Shift 1 Layout among in-service and Tarped machines. Carry and set
      down the Item with the Target brightening, and query the Operator and Counter components. Load the WASM build in
      a browser once and repeat the carry.
