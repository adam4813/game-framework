# Enhancement: Rattling tell before a Fault strikes

Status: needs-triage
Source: raised during [Decoder Faults & the Oil Can in Shift 2](../../signal-room/issues/06-condition-heat-overdrive.md), 2026-10-17

## Idea

In the slice, a Station Fault (Jam or Seize) strikes with no warning when its hidden cooldown runs out. This idea adds
a short **Rattling** tell, about 2 s of a per-machine rattle sound and smoke wisps, just before the Fault strikes. Players
could then hear which machine is about to go without looking, as the GDD's "Wear Cues" audio intends.

## Open questions

- Is the tell only a warning, or can acting during it prevent the Fault (e.g. oiling during Rattling stops a Seize)?
- Does the tell reveal which kind of Fault is coming?
- Is the length data per Fault kind?
- How it reads for AI Operators, and in the procedural audio palette (per-machine rattle).
