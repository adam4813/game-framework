# Room layout, movement & set-down

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: resolved
Blocked by: 01, 02

## Question

What do the Shift 1 and Shift 2 rooms look like and how do Operators move through them? Settle the floorplans (which stations, walls vs islands, Mainframe on the rug, tarped machines as obstacles), room scale relative to Operator speed (cross-room travel time is a tuning target), movement feel (inertia, dash multiplier and cooldown), Operator–Operator collision/blocking, the collision approach (Jolt character controller vs simple 2D shapes on the floor plane), the interaction range/facing rule for choosing a station, and whether rooms are authored as engine Level JSON.

Context: [Routine Cable route & station verbs](01-routine-cable-route.md) is resolved. Set-down is Counters + station slots only, never the floor; hand-offs go through Counters. Settle how many Counters each room has and where. The Keypunch stands in the room in both `keypunch` Modifier states (tarped/set dressing when unused).

Context from [Operators & Swap](02-operator-seats-coop-input.md): the camera is one fixed iso shot framing the whole floor, so the room must fit that frame.

## Answer

Resolved 2026-10-17 (grilling).

**Authoring: Shift → Layout.** A **Layout** is one engine Level JSON on a 1 m floor grid: walls, rug, Station and
Counter placements, dressing props and named spawn markers (`spawn_1`, `spawn_2`, …). A **Shift** names its Layout and
lists which of the Layout's Stations are in service. Any Station it doesn't list stands **Tarped**. Shifts 1 and 2
share one Layout, and later Shifts may point at others with no C++ change. The Roster refers to spawn markers by name.
The same grid feeds `GridNavGraph` for AI pathing. Furniture snaps to cells, but Operators move freely.

**Placement data.** Each Station placement in the Layout authors its footprint (in cells) and the sides it can be
worked from. Several Operators can stand at a Station, but only one Holds it at a time. Starting footprints: Radio 2×1
on the wall (worked from S), Keypunch 1×1 island, Decoder 2×2 on the rug (all 4 sides), Telex 1×1 island (W/N/E),
Cooling Vent 1×1 corner. Counters are 1×1 solid furniture with one spot, worked from every open side. They double as
short dividers.

**Shift 1–2 Layout** (13 × 9 cells inside the walls, camera looking north from the south edge; cells are a starting
point and tuning lives in the Layout file):

```
        0  1  2  3  4  5  6  7  8  9 10 11 12
     ════════════════════════════════════════   full-height north wall
  0  ║ C  R  R  C  ·  ·  ~  ~  ·  ·  x  x  x │
  1  ║ ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  · │
  2  ║ ·  ·  ·  K  ·  ·  ·  ·  ·  ·  ·  ·  · │
  3  x ·  ·  ·  ·  ·  ░  D  D  ░  ·  T  ·  · │
  4  x ·  ·  ·  ·  ·  ░  D  D  ░  ·  C  ·  · │
  5  ║ ·  1  ·  ·  ·  ·  ·  ·  ·  ·  C  2  · │
  6  ║ ·  ·  ·  ·  ·  ·  C  C  ·  ·  ·  ·  · │
  7  ║ ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  · │
  8  ║ V  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  · │   (open south edge, foreground desk)
     R Radio   K Keypunch   D Decoder on cosmetic rug ░   T Telex   C Counter ×6
     V Cooling Vent (Tarped in Shift 1)   ~ storm window   x future-machine tarps (props)   1/2 spawns
```

Walking distances: Radio→Keypunch ≈ 2.5 m, Keypunch→Decoder ≈ 4 m, Decoder→Telex ≈ 3 m, Decoder→Vent ≈ 7 m (on
purpose: the Shift 2 heat run pulls an Operator away). With `keypunch` off, the Keypunch stands Tarped and is just an
obstacle.

**Tarped vs props.** A Tarped Station keeps its footprint and is solid. It can't be targeted (the highlight skips it,
no sound), holds no Items, and is never an AI target. The `x` tarps for out-of-scope machines (Power Panel, Cipher
Rotors, Supply Rack) are plain dressing props with no Station data.

**Walls vs camera.** Wall cells carry a height class. The north wall is full height, the east and west walls are low,
and there is no south wall: the south edge is open, can't be walked into, and has a foreground desk edge as dressing.
No fade or cutaway code is needed.

**Movement** (all tuning data):
- Walk 4.5 m/s, with a cross-room run of about 3 s as the tuning target.
- Inertia: about 0.08 s to full speed and about 0.15 s to stop. Facing snaps toward input in about 0.06 s.
- Dash: 2.2× for 0.2 s with a 0.5 s cooldown, on Shift (no double-tap). It can be used while carrying, and it cancels
  any Hold, like a movement key.
- Each Item has a `carry_speed` multiplier on walk and dash, defaulting to 1.0× for every slice Item. It's a lever for
  future heavy or fragile Items.

**Collision.** Hand-rolled 2D on the floor plane: Operator circles vs. cell AABBs with slide-along-wall. No Jolt in the
slice. Between Operators, soft separation: overlapping circles push apart and a mover slides around. An Operator
working a Station is never pushed. AI yielding belongs to [AI Operator behaviour](08-ai-operator-behaviour.md).

**Interaction targeting.** The target is the cell in front of the Operator (a facing ray about 0.9 m from its center),
if that cell is a workable side of a Station or a Counter. Otherwise it's the nearest workable thing within 1.2 m
inside a ±50° cone. The target is always highlighted before pressing. A Hold stays locked to its target while held.
On any interaction the Operator turns to face the target but never snaps position, matching Overcooked. Position snap
is a possible later refinement.

Glossary: added **Decoder** (avoid "Mainframe"), **Layout** and **Tarped**.

## Comments

- 2026-10-17: [Decoder Faults & the Oil Can in Shift 2](06-condition-heat-overdrive.md) rules the Cooling Vent out of scope. Cell (0,8) becomes a Counter that starts with the **Oil Can** (a
  reusable tool Item) in Shift 2, so the ~7 m pull from the Decoder stays as the Seize-repair run. In Shift 1 it's a plain
  Counter with no can on it.
- 2026-10-17: [Radio presets & spectrum read](12-radio-tuning-prototype.md): Shift 2 puts **3 Radios** in service (Shift 1
  Tarps the two extras). Starting placements: all on the north wall, 2×1 worked from S, at cells (1–2,0), (4–5,0) and
  (8–9,0), with the Counter at (3,0) between the first two. Walking distances to the Keypunch change; tune in the
  Layout file.
- 2026-10-18: which tarp is which machine (all dressing props, no Station data). **Cipher Rotors** on the floor at
  (10–11,0), **Supply Rack** at (12,0) in the NE corner, and **Power Panel** mounted on the west wall beside rows 3–4,
  with no floor footprint. The `~` storm window is on the north wall above cells (6–7,0). Those two floor cells stay
  walkable, which gives a gap between Radio 2 and Radio 3. In Shift 2 the Keypunch at (3,2) stands Tarped unless the
  `keypunch` Modifier is on. Layout read: Radio 3 is about 1 m from the Decoder and Radio 1 about 5 m away, so picking
  a Radio has a cost. Row 1 is a clear lane in front of the Radios, and rows 5–7 are open floor.
- 2026-10-17: [Greybox readability & juice](15-greybox-readability.md) settles how the target reads. It **brightens**
  in the Seated Operator's identity colour, with no outline. While carrying, **where-to-go chevrons** mark every Station
  that accepts the held Item. Stations are small machines on tables or benches that fill their footprints, with state on the top or back.
