# Routine Cable route & station verbs

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: resolved
Blocked by: 

## Question

What exactly happens at each station on the Shift 1 Routine Cable route, and which items move between them? The GDD is inconsistent: the Shift 1 station list and floorplan include a **Keypunch** island (Tap, paper-jam repair), but the route table reads Radio → Decoder → Telex and Step 1 has the card keypunched *at the Radio*. Decide whether the Keypunch is a separate stop, and for Radio, Keypunch, Decoder and Telex settle the verb (Tap / Hold / Walk-away / Drift), the inputs and outputs, what an Operator can leave mid-job, and how a carried item is put down (counters? any surface? floor?). Also settle how a ticket binds to a card: is a card tied to a specific ticket from the moment it's tuned, or matched at the Telex?

## Answer

Resolved 2026-10-17 (grilling).

**Routing is item-driven, not a route recipe.** Each station declares **transforms** in data: required inputs → outputs,
a verb, and a duration. The "route" is just the chain the transforms form (the ticket rack still shows the dispatch type,
derived from that chain). Each (station, input set) maps to exactly one transform, so the result is deterministic.

**Station shape (all stations):**
- A transform may take N inputs and give M outputs. Inputs go in one at a time, and work starts when the set is
  complete. Outputs stay on the station until all are picked up. Only then can a new job start.
  One job per station, **never a queue**. The slice only uses 1→1.
- Every machine leaves its output on itself, so the Operator must **pick it up** (Interact, empty hands). The Radio
  works the same way.
- Once a job starts, its items are **locked in** until it's done. Progress belongs to the **station**, so an Operator
  can walk away and any Operator can resume.
- A station refuses items it has no transform for: a "no" buzz, and the item stays in hand.
- Carried stacks/bundles and machine "settings" (e.g. a cipher preset on the Decoder) are allowed by the model but
  left to later efforts.

**Stations in the slice:**

| Station | Transform | Verb | Notes |
|---|---|---|---|
| Radio | ∅ → Raw Card (or Morse Tape with the `keypunch` Modifier) | Hold to copy, ~2 s | Leaving pauses and keeps progress; changing preset resets it. Shift 1: one frequency, no presets. Shift 2: 5 presets on keys 1–5 jump to labelled spikes on a spectrum. Some spikes are decoys, and spikes and frequencies are Shift data. Tickets show a frequency label, never a preset number. The input layer also defines next/prev preset for a later gamepad. No needle drift in the slice (frequency drift is a Shift 6 hazard). |
| Keypunch | Morse Tape → Raw Card | Short Hold, ~1.5 s | Only with the `keypunch` Modifier. Otherwise it's set dressing. Paper jam deferred. |
| Decoder | Raw Card → Decoded Card | Walk-away 6 s, with Hold to crank it to ~2.5 s | Overdrive and wear belong to [Condition, heat & Overdrive in Shift 2](06-condition-heat-overdrive.md). Shift 1's scripted jam is a Decoder card jam (works in every Modifier combination). |
| Telex | Decoded Card → (transmit) | Place, then Tap to stamp | With the `telex_autostamp` Modifier the verb is Instant: placing it transmits. |

**Ticket matching:** an item carries the **frequency** it was copied from. Stamping at the Telex clears the most urgent
open ticket with that frequency. If nothing matches (an expired ticket or a decoy spike), it transmits for nothing: no
score and no extra penalty. No cap on items in flight. Stale items can still be completed; they just don't count.

**Set-down & hand-off:** items can only be set down on **Counters** (one item per counter spot) and in station slots,
never on the floor. Operators pass items only through counters (no throwing or direct pass). How many counters there
are and where they go belongs to [Room layout, movement & set-down](07-room-layout-movement.md).

**Modifiers:** a named set of data overrides (transforms, tuning values, Radio setup) that a Shift switches on as its
gimmicks. The dev toggle (imgui / title screen) forces Modifiers on or off over the Shift's declaration, so A/B tests
and Shift gimmicks share one mechanism. The first two are A/B tests: `keypunch` (1-machine vs 2-machine) and
`telex_autostamp`. Shift 1's single-frequency Radio is a candidate Modifier too. The override schema is part of the
Data schema fog.
