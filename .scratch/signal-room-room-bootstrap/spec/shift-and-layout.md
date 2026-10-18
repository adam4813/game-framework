# Spec 1 · Shift & Layout data

Part of [Signal Room spec 1: Room & bootstrap](../spec.md). The Shift and Layout file schemas, registries keyed by string, load errors and the room's scale.

## User stories

9. As a designer, I want a Shift file that names its Layout, so that several Shifts can share one floorplan.
10. As a designer, I want the Shift file to list which of the Layout's Stations are in service, so that every Station
    it doesn't list stands Tarped with no C++ change.
11. As a designer, I want the Layout authored as an engine Level JSON on a 1 m floor grid, so that it uses the engine's
    existing loader, names and refs.
12. As a designer, I want each Station placement to give its kind, cell, footprint and worked sides, so that I can move
    or resize a machine in data.
13. As a designer, I want Counters authored as 1×1 solid furniture worked from every open side, so that they double as
    short dividers.
14. As a designer, I want wall cells to carry a height class (full, low or none), so that the north wall stands tall,
    the side walls stay low and nothing hides the floor from the camera.
15. As a designer, I want the open south edge to block movement without a wall mesh, so that Operators can't walk off
    the floor and the camera's view stays clear.
16. As a designer, I want dressing props (future-machine tarps, the storm window, the foreground desk edge, the rug)
    placed in the Layout with no Station data, so that the room looks furnished without adding game rules.
17. As a designer, I want named spawn markers (`spawn_1`, `spawn_2`, …) in the Layout, so that a Shift's Roster can
    say where each Operator starts.
18. As a designer, I want the Shift's Roster to list one Operator with its spawn marker and identity colour, so that
    spec 3 can grow it to two without redefining it.
19. As a designer, I want the Shift to say which Items start on which Counters, so that this spec's placeholder Item
    (and later the Oil Can in Shift 2) is placed by data.
20. As a designer, I want the Layout to feed the same occupancy grid to the engine's `GridNavGraph`, so that the AI spec
    can path over the floor I authored.
21. As a designer, I want a clear load error naming the file and the bad entry (unknown Station name in the in-service
    list, a footprint off the grid, overlapping solid cells, a missing spawn marker), so that I can fix data quickly.
22. As a designer, I want the Shift 1–2 Layout to match the agreed 13×9 floorplan, with Radios at (1–2,0), (4–5,0) and
    (8–9,0), so that the first playable room is the one the design was tuned for.

## Implementation decisions

- **Shift file** (one per Shift): `id`, display `name`, `layout` (a path to a Layout file), `in_service` (Station
  placement names), `roster` (an ordered list; in this spec, one entry with `spawn` marker name and `colour` palette
  token), and `counter_items` (Counter placement name → Item id). Spec 2 adds Radios' frequencies, tickets, the
  Collection Schedule and Modifiers. Spec 3 grows the Roster. Spec 4 uses `counter_items` for the Oil Can.
- **Layout file** = an engine Level JSON with game-registered loaders:
  - a `floor_grid` singleton (`width` 13, `height` 9, `cell_size` 1.0 m, origin), which owns the occupancy grid;
  - `station` on a placement entity: `kind` (string key into the greybox catalogue: `radio`, `decoder`, `telex`,
    `keypunch`), `cell`, `footprint` (w×h in cells), `worked_sides` (subset of N/E/S/W). The entity name is the
    placement name the Shift refers to (for example `Radio1`, `Radio2`, `Radio3`, `Decoder`, `Telex`, `Keypunch`);
  - `counter` on a placement entity: `cell` (always 1×1, worked from every side that isn't solid);
  - `wall` runs: cells plus a `height` class (`full`, `low`), and an `edge_block` for the open south edge (it
    collides but has no mesh);
  - `prop`: `kind` (greybox catalogue key such as `tarp`, `storm_window`, `desk_edge`, `rug`), `cell`, `footprint`,
    and a `solid` flag. Wall-mounted props like the Power Panel tarp have no floor footprint;
  - `spawn_marker` with a `cell` (or a position) and a name.
  - The camera is an ordinary Layout entity using the engine `camera` loader and a `look_at` ref to a framing target,
    so retuning the shot is a data edit.
- **Kind keys are strings into registries, not enums.** Station kinds, prop kinds and Item ids are open-ended (Shifts
  3–10 add machines), so they are string-keyed registry entries, per `CODING_STANDARDS.md`.

## Scale

- 13×9 cells at 1 m, so the floor is 13 m × 9 m. At 4.5 m/s a full east-west run takes ≈ 2.9 s, which meets the ~3 s
  cross-room target. Machines fill their footprints on ~0.62 m tables. The camera at ~55° pitch fits the whole floor
  plus the north wall, as checked in the greybox prototype. Every number stays in the files above.
