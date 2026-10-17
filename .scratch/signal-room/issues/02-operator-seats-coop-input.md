# Operators & Swap

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: open
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
