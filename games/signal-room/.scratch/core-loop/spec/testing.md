# Spec 2 · Testing decisions

Part of [Signal Room spec 2: Core loop](../spec.md). The one seam (the Room harness, extended), what good tests check,
and the runtime exit check.

- **One seam: spec 1's headless `RoomHarness`, extended** (confirmed with the user). No new pure-function seams: the
  schedule generator, scoring, Modifier patching, the derived Briefing route and Hint choice are covered only through
  it. Extensions:
  - load a Shift with a **pinned seed** and optional **forced Modifiers**;
  - **step simulated seconds** (`StepSeconds(s)`) as well as frames, at the fixed delta;
  - hold the `interact` key across frames for Holds (already possible with `Hold`/`Release`);
  - small query helpers that read world state only: the Shift phase and Window left, open Tickets (state, frequency,
    gauge), score / par / chain / stars, a Station's Job phase and progress, what a Station or Counter holds, the
    coupler, chevron and Bubble state, and the visible HUD / Hint / Briefing / results UI entities and their label
    text.
  - Tests that only need the route skip the Briefing by pressing `interact` once, like a player.
- **Good tests check behaviour a player or designer would notice**, never internals. Examples:
  - "a Raw Card placed on the Decoder becomes a Decoded Card after 6 s, and after ~2.5 s while cranked";
  - "releasing the crank mid-Job keeps its progress";
  - "a Telex stamp with two On Air 7.0 Tickets clears the one with less time left";
  - "a 7.5 card stamped with only 7.0 Tickets open changes nothing";
  - "Holding at a Radio with no On Air Ticket refuses and the Operator gets a ✗";
  - "engaging Radio1 darkens Radio2, and both come back 0.4 s after the copy ends" (a fixture Shift with two Radios);
  - "a Ticket with 30% overfill stays Stand-by for 27 s, then On Air for 90 s, then is Lost: score −10, chain reset";
  - "a red-band Clear on chain step 2 scores round(20 × 0.5 × 1.5) = 15";
  - "with the rack full, a due Ticket waits; with it empty, the wait halves";
  - "with seed 42 the schedule produces the same Tickets twice";
  - "nothing goes On Air in the last 20 s, and at 0:00 the results show and Items are gone";
  - "Orientation: the clock stays at 4:00 until the first Clear, which then scores 20 and starts the Window";
  - "with `keypunch` forced on, the Radio gives a Morse Tape and the Briefing route lists the Keypunch";
  - "an override path that doesn't resolve fails the load naming the Modifier";
  - "the Hint over the Radio says Hold to copy, and retires after the first copy".
- **Fixtures:** Shift and data fixtures under the game's tests (for example a two-Radio Shift for the coupler, a Shift
  with a short Window and a dense schedule, a bad Modifier). Check every Layout position against
  `bunker.layout.json` or use a fixture Layout.
- **Prior art:** `games/signal-room/tests` (`RoomHarness`, `room_bootstrap_test`, `items_test`, `targeting_test`),
  registered with `add_signal_room_test`; `tests/scene_equivalence` for the harness pattern. The test count only
  grows (baseline 85 at the start of spec 2).
- **Runtime verification** (required by `AGENTS.md` before calling a ticket done): launch the desktop build (REST port
  27760), use the Flecs Remote API to drive input and query the new components for the feature, take a screenshot of
  any new visual read, and check the log for asserts. The **spec exit** (last ticket): one human clears Shift 1 from
  the Briefing through Orientation to the results screen with at least one star, on desktop and once in the WASM
  build in a browser.
