# Spec 2 · Stations, Transforms & Jobs

Part of [Signal Room spec 2: Core loop](../spec.md). The Station catalogue, route Items and their frequency, the four
verbs, Jobs, the interact rules and refusals.

## User stories

1. As a player, I want to place a Raw Card on the Decoder by pressing the action key while carrying it, so that
   decoding starts.
2. As a player, I want the Decoder to keep working when I walk away, so that I can do something else for its 6 s.
3. As a player, I want to Hold the action key at a running Decoder to crank it, so that it finishes in about 2.5 s
   when I have nothing better to do.
4. As a player, I want letting go of the crank to drop the Decoder back to its normal speed, without losing progress,
   so that cranking is never wasted.
5. As a player, I want the finished Decoded Card to wait on the Decoder until I pick it up empty-handed, so that a
   finished Job never vanishes.
6. As a player, I want to place a Decoded Card on the Telex and then press the action key to stamp it, so that the
   message is transmitted.
7. As a player, I want an Item I place on a Station to be locked in until the Job is done, so that a Job can't be
   half-undone.
8. As a player, I want a Station's progress to belong to the Station, so that any Operator can walk away and come back
   (or, from spec 3, another Operator can finish it).
9. As a player, I want a Station to run one Job at a time with no queue, so that a busy machine is plainly busy.
10. As a player, I want a Station that can't take the Item I carry to refuse it and leave it in my hands, so that a
    wrong press never loses an Item.
11. As a player, I want working a Station's verb (copy, crank, stamp, pick up) to need empty hands, so that a press
    while carrying always means "put this here".
12. As a player, I want a Hold to stay locked to its Station while I hold the key, so that my Target can't drift to a
    neighbour mid-Hold.
13. As a player, I want moving or dashing to end a Hold, so that I am never stuck working a machine.
14. As a player, I want every intercept Item to carry the frequency it was copied from, and show it as a coloured
    stripe, so that I can tell which Ticket a card belongs to.
15. As a player, I want a Raw Card, a Decoded Card and a Morse Tape to have distinct shapes, so that I can tell what I
    carry at a glance.
16. As a designer, I want each Station kind's Transforms (inputs, outputs, verb, duration) in a data file, so that new
    machines and route changes need no C++.
17. As a designer, I want a Transform to allow N inputs and M outputs (inputs placed one at a time, outputs all picked
    up before the next Job), so that later machines fit the same model, even though the slice only uses 1→1 and 0→1.
18. As a designer, I want a Transform's verb to be one of Instant, Tap, Hold or Walk-away, with its numbers in data,
    so that each machine's feel is tunable.
19. As a designer, I want each (Station kind, input set) to map to exactly one Transform, enforced at load, so that the
    result of placing an Item is always deterministic.

## Implementation decisions

- **Station catalogue** (new data file, string-keyed by Station kind, the same keys as the greybox catalogue's
  `stations`): each kind lists its Transforms. A Transform has an `id`, `inputs` (Item ids, may be empty), `outputs`
  (Item ids, may be empty), a `verb`, and verb numbers:
  - `instant`: the Job completes the moment its input set is complete.
  - `tap`: `taps` presses (empty-handed) complete it. Telex: 1.
  - `hold`: progress advances only while an Operator Holds the action key with the Station as its Target, for
    `duration` seconds. Radio: 2.0. Keypunch: 1.5.
  - `walk_away`: progress runs by itself for `duration` seconds once the input set is complete. An optional
    `crank_duration` makes it run at `duration / crank_duration` speed while an Operator Holds it. Decoder: 6.0 /
    crank 2.5.
  - `transmit: true` on a Transform marks that its completion sends a transmission (Telex). It consumes its inputs and
    has no outputs.
  Starting catalogue: `radio` (∅ → `raw_card`, hold 2.0), `keypunch` (`morse_tape` → `raw_card`, hold 1.5),
  `decoder` (`raw_card` → `decoded_card`, walk-away 6.0, crank 2.5), `telex` (`decoded_card` → transmit, tap 1).
  Load errors name the file and entry: an unknown Item id, an unknown verb, and two Transforms of one kind sharing an
  input set.
- **Item catalogue** gains `raw_card` (cream card), `decoded_card` (white card with a dark band) and `morse_tape`
  (spool), each with a greybox shape and a `frequency_stripe: true` flag. The `placeholder` Item stays for test
  fixtures; Shift 1's `counter_items` no longer places it, so harness tests that used it move to a fixture Shift.
- **Frequency on Items:** the `Item` component gains a `frequency` label (empty for tools). A Radio's outputs take the
  Radio's frequency. Any other Transform's outputs take the frequency of its first input that has one. The greybox
  draws the stripe in the frequency's colour (see [Radios](radios-and-coupler.md) for the frequency table).
- **Shift `counter_items`** entries may also be an object `{ "item": <id>, "frequency": <label> }`, so a Shift (or a
  test fixture) can start a striped card on a Counter. The plain string form stays valid.
- **Jobs.** A Station's Job state lives on the Station entity (reflected, so the Explorer shows it): the active
  Transform, progress 0–1, and a phase: **Idle** → **Loading** (some inputs placed; only for N > 1) → **Running** →
  **Done** (outputs waiting) → Idle once every output is picked up. A Transform with no inputs (Radio) goes Idle →
  Running when the first Hold starts. Items placed on a Station and outputs waiting on it use the same `(Holds, item)`
  relationship as Counters, so every Item still has exactly one holder.
- **Interact rules** (on the `interact` press, for the human's Operator and its Target):
  - **Carrying, Target is a Station:** place the Item if the Station is Idle or Loading and some Transform of its kind
    takes that Item into a free input slot. Otherwise refuse.
  - **Empty-handed, Target is a Station:** pick up an output if the Job is Done. Otherwise start the verb: a `tap`
    press counts, a `hold` or a `walk_away` crank starts a Hold. Otherwise (nothing to do) refuse.
  - **Counters** keep spec 1's pick-up and set-down. A press at a Counter that can't do either now refuses too.
  - **No Target:** nothing happens.
- **Holds.** An Operator Holds while the `interact` action stays down after the starting press. While Holding, its
  Target is frozen to that Station. Releasing the key, any move input or a dash ends the Hold (a Hold-verb Job keeps
  its progress; a crank just stops). One Operator Holds a Station at a time; a second Operator's press there refuses.
  Holding is a reflected component on the Operator (which Station, since when).
- **Refusal** is one event (Operator, Target, reason) that the Bubble shows as a ✗ ([Machine reads](machine-reads.md))
  and the audio spec will hook. It never changes the Operator, the Item or the Station.
- **Transmission** is an event (frequency, Station) raised when a `transmit` Transform completes. The Ticket rules
  consume it ([Tickets](tickets-and-schedule.md)). With no matching Ticket it transmits for nothing: no score, no
  penalty.
- **Tarped** Stations have no Job state and are never Targets (unchanged from spec 1).
