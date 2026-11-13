# 11: Hints

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [Hints](../spec/hints.md), [Machine reads & guidance](../spec/machine-reads.md) (the shared acceptance check), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** Contextual Hints teach the route by playing it. A data-driven, ordered rule table picks one Hint over
the Station that is the human's next step (place the held card where it goes, else stamp / crank / pick up what's
waiting, else Hold to copy at the Radio for the oldest On Air Ticket), showing the bound key and a short verb. A Dash
Hint floats over the Operator the first time it carries something. Each Hint kind retires for the attempt once the
human has done that step.

**Blocked by:** 03 (Radios & the Antenna Coupler), 08 (Where-to-go chevrons & the Bubble ✗), 10 (Orientation).

**Status:** ready-for-agent

- [ ] A Hint rules data file with the spec's fields and fixed `when` vocabulary (`hands`, `station_kind`,
      `station_state`, `accepts_held_item`, `radio_for_oldest_on_air`); unknown keys or values are load errors naming
      the rule.
- [ ] Action events for retiring (`place`, `pick_up`, `hold`, `tap` at a Station kind, and `dash`) raised by the human's
      Operator. Retire counts per attempt; reset at attempt start.
- [ ] Choice per frame for the human's own Operator: first live `station` rule matching an in-service Station (nearest
      on ties), plus at most one `operator` Hint. `accepts_held_item` uses ticket 08's acceptance check.
- [ ] The slice rule set from the spec, including the Keypunch rules for the `keypunch` Modifier.
- [ ] Rendering: engine UI panel (key glyph from the bindings' first key for the action + text), projected above the
      anchor; shown only in Orientation and Running; offsets and sizes in HUD data.
- [ ] Harness tests (query Hint UI entities and their text):
  - in Orientation, empty-handed, the Hint is over Radio1 and reads the interact key + "Hold to copy";
  - after the copy, carrying the card, the Hint moves to the Decoder ("place") and a Dash Hint shows over the
    Operator; a dash retires the Dash Hint;
  - after the first copy the copy Hint never shows again this attempt; Retry brings it back;
  - rebinding `interact` in a fixture bindings file changes the glyph;
  - a rule with an unknown `station_state` fails to load.
- [ ] Runtime: desktop build; screenshot the copy Hint over the Radio and the place Hint over the Decoder. No asserts.

## Comments
