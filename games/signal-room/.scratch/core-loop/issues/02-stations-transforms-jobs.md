# 02: Stations, Transforms & Jobs (Decoder and Telex)

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [Stations, Transforms & Jobs](../spec/stations-and-transforms.md), [Tickets, Window & Collection Schedule](../spec/tickets-and-schedule.md) (Matching only), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** The machines start working. A data-driven Station catalogue gives each Station kind its
Transforms. Carrying a Raw Card to the Decoder and pressing places it. The Decoder runs by itself for 6 s, or about
2.5 s while the player Holds to crank, and the Decoded Card waits on it until picked up empty-handed. Placing that
card on the Telex and pressing once stamps it, which raises a transmission and clears the matching Ticket from
ticket 01. Inputs lock in, progress belongs to the Station, one Job at a time, working needs empty hands, and any
press that can't do anything raises a refusal event (the ✗ shows in ticket 08). This is the core model the rest of
the spec builds on: keep the verbs generic (the Radio's Hold in ticket 03 and the Keypunch in ticket 05 reuse them).

**Blocked by:** 01 (Tickets, the Window & the Collection Schedule).

**Status:** done (follow-ups: Radio frequency on outputs is ticket 03; Hold/Pausable not yet used by any room system; same-frame double Hold race is latent until spec 3)

- [ ] A Station catalogue data file, keyed by Station kind, lists Transforms (`id`, `inputs`, `outputs`, `verb`,
      `duration` / `taps` / `crank_duration`, `transmit`, plus the kind's `display_name`), with the spec's four kinds
      (`radio`, `keypunch`, `decoder`, `telex`). Load errors name the entry for an unknown Item or verb, and for two
      Transforms of one kind sharing an input set.
- [ ] The Item catalogue gains `raw_card`, `decoded_card` and `morse_tape` with greybox shapes and a frequency stripe.
      `Item` gains a `frequency` label, and outputs inherit it per the spec. Shift `counter_items` also accepts
      `{item, frequency}` objects.
- [ ] Shift 1 no longer places the placeholder Item. Tests that used it move to a fixture Shift.
- [ ] Job state on each in-service Station (phase Idle / Loading / Running / Done, Transform, progress), reflected.
      Station inputs and waiting outputs use `(Holds, item)`, so every Item keeps exactly one holder.
- [ ] Verbs `instant`, `tap`, `hold` and `walk_away` (with crank), as the spec section defines them. The `hold`
      machinery is generic: ticket 03's Radio uses it with no inputs.
- [ ] Interact rules: carrying → place, or refuse; empty-handed → pick up a Done output, else start the verb, else
      refuse; Counters keep spec 1's behaviour and refuse a press that can't act; no Target → nothing.
- [ ] Holds: `interact` held after the starting press; the Target is frozen while Holding; release, any move input or
      a dash ends it; one Holder per Station. A `Holding` component on the Operator, reflected.
- [ ] A refusal event (Operator, Target, reason) and a transmission event (frequency, Station). Completing the Telex
      Transform raises the transmission and destroys the card.
- [ ] Transform durations are editable live in the Room panel.
- [ ] Harness tests (fixture Shift with a 7.0 Raw Card on a Counter near the Decoder, using real Layout positions):
  - placing it on the Decoder and waiting 6 s gives a Decoded Card on the Decoder, carrying 7.0;
  - Holding at the running Decoder finishes it in about 2.5 s; releasing mid-crank keeps the progress;
  - the Decoded Card can't be picked up while carrying, and can be once empty-handed;
  - a second Raw Card is refused at a busy Decoder and stays carried;
  - a Decoded Card offered to the Decoder is refused;
  - placing a Decoded Card on the Telex and pressing once clears an On Air 7.0 Ticket (spawned through the harness);
  - moving during a crank ends the Hold;
  - pressing empty-handed at an idle Decoder raises a refusal;
  - a Station catalogue with a duplicate input set fails to load naming the kind.
- [ ] Runtime: desktop build; via the Remote API carry a fixture card through the Decoder and Telex and watch the
      Ticket clear. No asserts.

## Comments
