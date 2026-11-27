# 09: Shift flow: the Briefing card, results and Retry

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [Shift flow, Briefing & Orientation](../spec/shift-flow.md) (all but Orientation), [Debugging & tuning](../spec/debugging.md), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** A Shift now has a beginning and an end. It opens on a Briefing card over the paused room: the
Shift's name, its route (derived from the in-service Stations' Transforms, so a Modifier that adds the Keypunch shows
it) and its notes. Pressing the action key starts the Shift. At the hard stop a results screen shows score, par,
stars, Tickets cleared and lost and the best chain, with a Retry that starts a fresh attempt. Both screens pause the
Room's simulation through pause-as-data.

**Blocked by:** 04 (Scoring, the Handler Chain & stars).

**Status:** done

- [x] Shift phases `Briefing` → `Running` → `Ended` (ticket 10 inserts `Orientation`), with a `ShiftPhaseChanged`
      event. Attempt start (load, Reload, Retry) resets Tickets, score and seed (unless pinned) and enters Briefing.
- [x] Briefing and Ended pause the Room's simulation systems (ADR 0004): Operators can't move, Jobs and gauges hold.
      Render, UI and the panel keep running.
- [x] Briefing card (engine UI `Modal`): name, derived route (Station `display_name`s joined by arrows), and the Shift's
      `briefing.notes`. The `interact` press or its button dismisses it. Shift 1 gets notes on the rack and gauge.
- [x] Results screen (engine UI `Modal`): score, par, stars (★/☆), Cleared, Lost Intercepts, best chain, and Retry
      (`interact` presses it).
- [x] Panel: Skip Briefing and End Shift buttons.
- [x] Harness tests:
  - Shift 1 loads into Briefing; walking keys don't move the Operator; one `interact` press enters Running;
  - the Briefing route reads "Radio → Decoder → Telex" (ticket 12 adds the Keypunch variant once Modifiers exist);
  - at 0:00 the results screen is visible with the score, par and stars labels matching the score singleton;
  - Retry starts a new attempt at Briefing with score 0 and an empty rack;
  - Reload from the panel also returns to Briefing.
- [x] Update existing harness tests that assume the room starts live: they press `interact` once to dismiss the
      Briefing, as a player would.
- [x] Runtime: desktop build; screenshot the Briefing and the results screen (End Shift from the panel), press Retry
      via the Remote API. No asserts.

## Comments

- Tests: 154 → 159 (5 new ShiftFlow tests). The harness starts the Shift on load by default (`KeepBriefing()` opts out), so
  existing tests did not need a key press each; no frame passes, so their timings are unchanged.
- The default raylib font is ASCII only, so the route arrows and stars are `>` and `*`/`-` (data in `hud.json`), not
  → and ★/☆. Swap them in the data once a font with those glyphs ships.
- A fresh attempt's Briefing is paused one frame late: the engine's transform propagation is Pausable, so pausing on
  frame 0 left the Room undrawn behind the card.
- Retry reloads from the deferred task queue; a Reload inside a system builds the new Room before the old one is gone.
