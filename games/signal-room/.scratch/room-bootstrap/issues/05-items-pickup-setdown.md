# 05: Items: pick up, carry & set down

Spec: [Signal Room spec 1: Room & bootstrap](../spec.md). Read: [Targeting, pick-up & set-down](../spec/targeting-and-items.md), [Camera & greybox reads](../spec/greybox-and-camera.md), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (this game's main: branch off it, PR back into it, never into `main`)

**What to build:** A placeholder Item waits on a Counter in Shift 1. Empty-handed at that Counter, the player presses
the action key to pick it up. The Item is then shown in front of the Operator (cards tilted toward the camera). They
can carry it, dash while carrying, and set it down on any free Counter with a dust puff. Wrong presses are harmless,
and an Item is never on the floor. This ticket closes spec 1: its exit check runs here.

**Blocked by:** 04 (Targeting & the Target brighten).

**Status:** done

- [x] An Item catalogue (data) gives each Item an `id`, a greybox `shape` key, `carry_speed` (default 1.0) and
      `tilt_to_camera`. Shift 1 uses one placeholder Item.
- [x] The Shift file gains `counter_items` (Counter placement name → Item id). An unknown Counter or Item id is a load
      error naming it.
- [x] Interaction runs on the `interact` press event. Empty-handed at a Counter holding an Item → pick up. Carrying at
      an empty Counter → set down. Anything else (an occupied Counter while carrying, an empty Counter while
      empty-handed, an in-service Station) does nothing.
- [x] Carrying and holding are relationships. Every Item has exactly one holder (an Operator or a Counter), and there
      is no floor state.
- [x] On interacting, the Operator turns to face the Target and never snaps position.
- [x] The carried Item's `carry_speed` scales walk and dash.
- [x] A set-down plays a dust-puff burst from the particles module, with intensity in data.
- [x] Harness tests:
  - pick up → walk → set down moves the Item to the second Counter;
  - setting down on an occupied Counter refuses, and the Item stays carried;
  - pressing at an in-service Station does nothing;
  - dashing while carrying works;
  - an Item with `carry_speed` 0.5 halves the walk distance;
  - Reload restores the starting `counter_items`.
- [x] Spec 1 exit (runtime, per `AGENTS.md`): in the desktop build, through the Flecs Remote API, switch to the Room
      scene, then walk, dash and collide through the Shift 1 Layout among in-service and Tarped machines. Carry and set
      down the Item with the Target brightening, and query the Operator and Counter components. Load the WASM build in
      a browser once and repeat the carry.

## Comments

**2026-10-18 (implementation):** Done on `feat/signal-room-05-items`.

- Data: `assets/items.json` is the Item catalogue (`placeholder`: shape `card_bundle`, `carry_speed` 1.0,
  `tilt_to_camera` true). Greybox shapes live in the catalogue's new `items` section, colours in the palette
  (`item_card`, `item_band`, `item_tag`, `dust`). Shift 1's `counter_items` puts it on `CounterSouthLeft`. The dust
  puff is the tuning file's `dust_puff` section (count, speed, lifetime, size, spread), live on the Room panel.
- Relationships: `(Carries, item)` on the Operator and `(Holds, item)` on the Counter, both exclusive. The one
  transfer path (`ItemInteraction`) moves the pair, so each Item keeps exactly one holder; the harness counts them.
- `Operator::carrySpeed` is gone: OperatorMovement reads the carried Item's `carry_speed` through `(Carries, *)`, so
  the old movement test's carry-speed half moved to `items_test.cpp`.
- The Item sits at the Counter model's `Top()` (derived from the catalogue), and `CarriedItemPlacement` shows a
  carried Item ahead of its Operator, tilted toward the camera for cards.
- Each Counter has an idle `dust` emitter child. A set-down bursts it by setting the emitter's spawn debt to the
  particle count with `rate` 0, so no engine change was needed. The ticket is game-only.
- The Operator turns to face the nearest point of the Counter's cell. Tests: 73 → 85 (12 new Items tests; the
  movement carry-speed test became a live-tuning-only test).
- Runtime (desktop, Remote API on 27760): walked into the in-service Radio1 (stopped at y 1.30) and dashed into the
  Decoder (x 5.70). Hit the Tarped Keypunch head-on (x 2.70, no Target). Picked up `Item1` from CounterSouthLeft,
  dashed while carrying, and set it down on CounterSouthRight, with the Target brightened. Queried the Operator's
  `Carries`/`Targets` pairs and the Counters' `Holds`; 14 dust particles spawned under `CounterSouthRight.dust`. No
  errors in the log. WASM: drove headless Chrome with DevTools key events, picked up, dashed and set down on
  CounterEast with no console errors.
- Review: fixed the duplicated Counter height (now `GreyboxModel::Top()`), the duplicated nearest-point maths (now
  `FloorGrid::ToNearestPointOf`), a shadowed `items` in `BuildRoom`, and the Shift's pair-of-strings (now
  `CounterItem`). The rest is filed as [backlog 17](../../backlog/issues/17-items-follow-ups.md), including the
  two-Operators-one-frame race.

**Retro:**

- Smooth: the harness and Remote API made every check concrete. The first tracer test went green on the first build.
- Friction: the screenshot tool takes ~1.7 s, longer than the 0.45 s puff, so seeing the dust meant stretching its
  lifetime live. The default 0.07 m particles read faintly, so the default size went to 0.1 m (a feel call for
  ticket 11/14-style review).
- Friction: the local clang-format reflows lines it didn't touch (see [backlog 13](../../../../../.scratch/backlog/issues/13-format-check-before-done.md)).
  The reflows were reverted by hand to keep this diff on the ticket.
