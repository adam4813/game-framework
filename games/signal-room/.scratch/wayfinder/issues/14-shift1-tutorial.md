# Shift 1 tutorial

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: resolved
Blocked by:

## Question

How much onboarding does Shift 1 need, and what shape does it take? Shift 1 teaches only the route (Radio copy →
Decoder → Telex), with no Faults. Options run from a briefing card alone, through contextual first-time prompts, to
gated steps (a scripted first Ticket, a paused Window until the first copy). Decide what's taught, how it's surfaced
(card, world prompts, HUD callouts), whether any step is gated or the Window runs from the start, and whether it
replays or is first-time only per Campaign slot. This gates spec 2 (Core loop); see the packets in
[Spec chunking & hand-off order](13-spec-chunking-order.md).

## Answer

Resolved 2026-10-17 (grilling). All numbers are starting values and live in data.

**Shape: Briefing + Hints, no scripted steps.** Onboarding is a Briefing card before the Shift and contextual Hints in
play. There is no step-by-step script; the one gated beat is Orientation (below).

**Briefing.** A card before the Shift showing its route (Radio → Decoder → Telex in Shift 1; what's new in later
Shifts). It shows on its own until the Campaign slot has **any result** for that Shift (derived from the saved per-Shift
best, no new field). After that it shows only from a **Briefing** button on the Shift picker, which works for any Shift.

**Hints.** One settings toggle (settings slot, default **on**), applying to **every Shift**.
- A Hint is a data rule: a Station + state (or Operator state) → key glyph + short text, floating over the target.
- Hints follow the **human's own Operator**: the next step for the Item it holds, else the Radio for the oldest On Air
  Ticket. After a Swap they follow the new Operator.
- Each Hint kind **retires for the attempt after the human does it once** (`retire_after: 1` in data). Steps an AI
  Operator does don't count.
- Slice set: the route verbs (Hold to copy at the Radio, place at the Decoder, crank/walk-away, pick up the Decoded
  Card, place + stamp at the Telex); **Dash**, shown on the first carry and retired on the first dash; **Swap**, shown
  when the second Operator clocks on. Shift 2's Fault reads (Mash a Jam, bring the Oil Can to a Seize) come from the
  same rule table. There is no HUD callout for the rack; the Briefing covers the rack and the gauge.

**Orientation (held opening).** Supersedes "the Window runs from the start" for Shift 1
([Tickets, window & scoring](05-tickets-window-scoring.md)).
- A Shift data flag (e.g. `orientation: true`); only **Shift 1** sets it in the slice.
- **Applies when** the Campaign's best on that Shift is **below 1★**, or whenever **2+ humans** are seated (slice 2),
  whatever the stars. Otherwise the Shift starts normally at 0:00.
- **While it holds:** the Window clock, the Collection Schedule and the first Ticket's gauge are frozen. A **pinned
  first Ticket** (NORMAL) arrives **On Air at once** (no overfill) and **can't be lost**.
- **On the first Clear:** that Ticket scores as a green Clear, counts toward par and starts the Handler Chain. The
  Window, the schedule and the **AI Operator** all start. The AI Operator walks in from a spawn marker by the door.
- **Not held** (the slot has cleared the Shift): the AI Operator and the Window start at 0:00.
- The Hints toggle has **no effect** on Orientation; Orientation is progression, Hints are presentation.

**Spec split** (updates the packets in [Spec chunking & hand-off order](13-spec-chunking-order.md)):
- **Spec 2:** the Briefing card (always shown, since there's no save yet), the Hint rule table and its route-verb and
  Dash Hints, Orientation's freeze and pinned Ticket (always applied).
- **Spec 3:** the AI Operator clocks on at the end of Orientation (a Roster entry joins then), and the Swap Hint.
- **Spec 4:** Fault Hints.
- **Spec 5:** the Hints toggle in the settings slot, the Shift picker's Briefing button, and both slot-derived rules
  (Briefing auto-show, Orientation's 1★ check).

Glossary: **Briefing**, **Hint** and **Orientation** added to `games/signal-room/GLOSSARY.md`.
