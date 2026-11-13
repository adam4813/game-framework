# DEFCON's role in a two-Shift slice

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: resolved
Blocked by: 

## Question

The GDD uses DEFCON only as Shift 10's in-shift escalation clock and as wall-board flavour, but the slice must carry DEFCON. What is DEFCON in the slice: a campaign-level meter driven by performance across Shifts, an in-shift escalation present from Shift 1, a difficulty/night-type selector (Lull/Normal/Peak), or flavour only? What moves it, what it changes (window, ticket density, wear rate, briefing text, room tint), and how the slice shows it.

## Answer

Resolved 2026-10-17 (grilling): **ruled out of scope.**

**What DEFCON is in the GDD.** Not a mission select. It appears only as (1) Shift 10 "DEFCON 1", a finale/boss Shift
whose clock escalates 5 → 1 and switches on one more hazard per step (wear, storm/jamming, blast doors/saboteur,
double FLASH), and (2) period flavour: a wall DEFCON Board that "escalates and tints the room". Both depend on content
from Shifts 3–10, so neither belongs in a Shifts 1–2 slice.

**Destination amended.** "DEFCON" came into the Destination when it was read as the mission select. It is replaced by
the **Shift picker / campaign flow**, which stays in the map's Screens & HUD fog until requisitions and campaign save
resolve.

**No board in the slice.** No DEFCON Board, room tint or step alert, not even as static set dressing.

**Carried forward for whoever specs Shift 10.** These were explored but not adopted:
- Preferred model: a **scripted in-Shift escalation**. The Shift sets a starting DEFCON plus timed steps, and each step
  switches on a Modifier mid-Shift. This reuses the Modifier mechanism and stays deterministic, so par and stars remain
  fair. Rejected alternatives: performance-driven (death-spiral risk), a campaign meter, a fixed night-type label.
- Preferred presentation (user-confirmed): **Board + tint + step alert**. A DEFCON Board prop on the wall shows the
  number, the room's light tint shifts per level, a step plays a klaxon plus a short banner naming what changed, and the
  briefing card states the starting DEFCON. No HUD widget, matching the in-world HUD fiction.
