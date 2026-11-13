# Decoder Faults & the Oil Can in Shift 2

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: resolved
Blocked by: 01

## Question

_Originally titled "Condition, heat & Overdrive in Shift 2"._

Define Shift 2's maintenance layer with only the Shift 1–2 stations available (no Supply Rack, so where do oil cans / spares come from?). Which machines have a Condition meter, what wears them, the Healthy → Rattling → Fault stages, which faults and repairs exist in the slice (gear jam, loose tube, paper jam, stuck valve; blown fuse needs spares, so is it in or out?), the core-temperature model and its thresholds, the Cooling station's controls (fan speeds, steam purge, coolant), Overdrive's risk/reward, and Shift 2's 'Mainframe runs hot' flaky-machine twist. Also: does Shift 1 have any wear beyond the scripted jam?

Context: [Routine Cable route & station verbs](01-routine-cable-route.md) is resolved. The Decoder is Walk-away 6 s with Hold-to-crank (~2.5 s); Overdrive builds on the crank. Shift 1's scripted jam is a Decoder card jam. The Keypunch only exists under the `keypunch` Modifier, so its paper jam is not in the slice. Wear/heat tuning should be expressible as Modifier overrides.

## Answer

Resolved 2026-10-17 (grilling). The ticket was renamed from "Condition, heat & Overdrive in Shift 2". The answer
dropped Condition, heat and Overdrive from the slice.

**No Condition meter, no heat, no Overdrive.** Wear isn't tracked. A Station breaks down through random **Faults**
instead. The Healthy → Rattling → Fault stages, core temperature, thermal runaway and Overdrive are all out of the
slice (see the map's Out of scope). The Decoder's Hold-to-crank (~2.5 s) stays as it is.

**Which Stations fault.** Only the **Decoder** in the slice. Fault data is per Station, so later Shifts can give
other Stations Faults with no C++ change.

**When a Fault strikes** (all tuning data, seeded per attempt like the Collection Schedule):
- Only while the Station is running a Job. An idle Decoder never faults, so the more it's used the sooner it breaks.
- A hidden cooldown is drawn at Shift start and after each clear, counted in **Job run time** (starting range 25–45 s).
  When it runs out, the next Fault strikes.
- The kind is picked by weight (starting Jam 50 / Seize 50). One Fault at a time.
- No warning before it strikes. A Rattling tell is backlogged as an enhancement
  ([Rattling tell before a Fault](../../backlog/issues/02-fault-rattling-tell.md)).
- A Fault stops the Job and cancels any crank Hold in progress. Cranking can't resume until it's cleared. A Faulted
  Station shows a red strobe, an alarm and an icon for its fix (gear = Jam, oil drop = Seize).

**The two Faults:**

| Fault | Effect on the Job | Repair |
|---|---|---|
| **Jam** | Progress drops to 0; the card stays locked in | "Use" the machine: Mash Interact 3× to yank the clearance lever. The Job restarts from 0. |
| **Seize** | Pauses at its current progress | Hold ~1.5 s at the Decoder while carrying the Oil Can; the Job resumes. |

Empty-handed Hold at the Decoder is still the crank. Holding while carrying the Oil Can is the Seize repair.

**Oil Can.** A reusable tool Item, never used up, with no stock and no Supply Rack. It starts on a Counter at cell
(0,8), the corner the Room layout had given the Cooling Vent, so the ~7 m pull away from the Decoder survives. Like any
Item it fills the carrier's one hand slot. It is absent in Shift 1.

**By Shift:**
- **Shift 1:** no Faults at all (Mario-style: teach the route first). The scripted tutorial Jam from
  [Routine Cable route & station verbs](01-routine-cable-route.md) is **dropped**.
- **Shift 2:** Faults are its flaky-machine twist (fiction: the Decoder runs on cheap replacement tubes), switched on by
  Shift data. Its **first Fault is pinned**: a Seize at ~20 s of Job run time, which teaches the Oil Can. After that
  the random cooldown takes over. Faults and their tuning must be expressible as Modifier overrides.

**Ruled out of scope:** Overdrive (it reads as a boost, and boosts are already out); the Cooling Vent, core temperature
and thermal runaway (the Vent doesn't fit the theme for now; a possible future idea); Loose Tube, Blown Fuse (needs
spares), Stuck Valve (needs the Vent), the Coolant Canister and the Keypunch paper jam.

**Backlogged during this session:** [Near-miss blast knocks power cables loose](../../backlog/issues/01-near-miss-power-cables.md)
(a world-caused event as a later Shift's signature gimmick) and [Rattling tell before a Fault](../../backlog/issues/02-fault-rattling-tell.md).
