# Spike: near-miss blast knocks power cables loose

Status: needs-triage
Source: raised during [Decoder Faults & the Oil Can in Shift 2](../../signal-room/issues/06-condition-heat-overdrive.md), 2026-10-17

## Idea

A room-wide **world event**, similar to a Fault but caused by the outside world rather than a Station's own breakdown.
A missile or shell lands nearby: the room shakes and power cables are knocked loose, so one or more Stations go dead
until an Operator plugs the cable back in.

It is meant to be a Shift's **signature gimmick**: one Shift is built around it, the way the GDD gives each Shift one
flaky machine.

## Open questions

- Terminology: what do we call a world-caused event, as opposed to a Station Fault? The GDD's "crisis" is one
  candidate.
- Is it scheduled like the Collection Schedule (pinned times) or random with a cooldown, like slice Faults?
- What gets unplugged: the Station's power cable itself, or a junction box or patch panel somewhere else? Is there a
  warning beforehand (a distant rumble or siren)?
- Is the fix a Hold on the cable end, or carrying a loose cable to its socket? Does it interact with the Power Panel
  (GDD Hazard 2, blackouts)?
- Juice: camera shake, dust falling, lights flickering (already in the GDD's expansion notes).

Out of scope for the Signal Room vertical slice (Shifts 1–2); a candidate for a later Shift.
