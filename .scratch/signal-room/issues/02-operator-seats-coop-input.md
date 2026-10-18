# Operators & Swap

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: resolved
Blocked by: 

## Question

Slice 1 is **single player on keyboard + helper AI Operator(s)**; human co-op (split keyboard / gamepads) is slice 2.
Within that, settle: how many Operators a Shift has (fixed 2? set per Shift in data?), which Operator the human starts
on, what Swap does exactly (input, which Operator you go to with 3+, cooldown or transition, what "finish what the
human was doing" means when the human was mid-carry, mid-crank or mid-repair), how the human tells at a glance which
Operator they control, and how one fixed iso camera frames the room. Also name the seam slice 2 will extend: what
"a human controls this Operator" looks like so a second human slots in without reworking the AI. Output: the
Operator/controller model the AI Operator spec builds on.

Context: [Gamepad & per-Operator input through Platform](03-gamepad-multi-input-research.md) is resolved, but gamepad
support is out of scope for slice 1. For slice 2: At most
keyboard + 3 pads, a split keyboard is a binding concern, web pads need a "press to join" step, and bindings must
survive reconnects.

## Answer

Resolved 2026-10-17 (grilling).

**Seat model (the slice-2 seam).** A **Seat** is one human's place at the console: their controls plus the one
Operator they currently hold. An Operator no Seat holds is an **AI Operator**, so AI is the default, not a controller
type you attach. Slice 1 has one keyboard Seat. Slice 2 adds Seats (pads, split keyboard) without reworking the AI.
In ECS terms, a Seat holds a relationship to its Operator.

**Roster.** Each Shift declares an ordered Roster in data (Operator count, spawn spots, identity colors). Shifts 1
and 2 both use 2 Operators. The Seat starts on Roster slot 1.

**Swap.** One key moves the Seat to the next Operator in Roster order that no other Seat holds, wrapping around.
Instant, no cooldown, with a short pulse cue on the Operator you arrive at. With 2 Operators it's a toggle. With as many
Seats as Operators, Swap does nothing.

**Leaving an Operator (hand-off).** The Operator left behind finishes what it was in the middle of:
- Mid-Hold (copying, cranking): it keeps holding until the Job is done.
- Mid-carry: it takes the Item to the Station whose Transform accepts it, or to the nearest free Counter if that
  Station is busy. If neither is free, it waits at the target Station still carrying, and loads the Item once the
  Station's outputs are picked up.
- Empty-handed: nothing to finish.
Then it picks its own next job (see [AI Operator behaviour](08-ai-operator-behaviour.md)). Job progress belongs to the
Station, so nothing is lost.

**Arriving on an Operator.** The AI's action stops, and from then on the Operator responds only to the Seat's input.
If it was mid-Hold, a short **coyote time** (~0.25 s, tuning data) keeps the Hold going: press the action key inside it
to continue without a break. A movement key ends it at once.

**Telling who you control.** Identity color belongs to the Operator (P1 brick red, P2 mustard). The Seat adds a floor
ring and a chevron above the head, snapping over with a pulse on Swap. In slice 2 each Seat gets its own ring color.
Each AI Operator shows a small intent icon naming its target Station.

**Camera.** One fixed high-angle iso shot that fits the whole floor and never moves. Camera shake is an offset on top.
Room size is limited by this frame (see [Room layout, movement & set-down](07-room-layout-movement.md)).

Glossary: added **Roster** and **Seat**; reworded **AI Operator** and **Swap**.

## Comments

- 2026-10-17: [Greybox readability & juice](15-greybox-readability.md) replaces the AI intent icon with the Operator
  **Bubble** (glyph naming the target Station; also shows ✗ on a refused action, emotes later). Each Operator wears an
  identity-colour collar. Camera shake is dropped from the slice: the camera stays dead still.
