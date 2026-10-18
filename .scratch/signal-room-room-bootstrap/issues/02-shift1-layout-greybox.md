# 02: Shift 1 Layout & greybox reads

Spec: [Signal Room spec 1: Room & bootstrap](../spec.md)
Base branch: `feat/signal-room` (this game's main: branch off it, PR back into it, never into `main`)

**What to build:** Launching the game shows Shift 1's 13×9 room as designed in
[Room layout, movement & set-down](../../signal-room/issues/07-room-layout-movement.md), including its comments
(three Radio placements, the Counter at (0,8) replacing the Vent, and which tarps are which machines). In-service
Stations (Radio1, Decoder, Telex) stand as greybox machines on tables with idle lamps and risers. Every other Station
(Radio2, Radio3, Keypunch) stands Tarped. Walls, the rug, the storm window, the foreground desk edge and the dressing
tarps are in place. A designer can move a machine, list another Station as in service, or recolour something in JSON
and see it after Reload. Shapes and colours follow
[Greybox readability & juice](../../signal-room/issues/15-greybox-readability.md).

**Blocked by:** 01 (Game Target & headless Room harness).

**Status:** ready-for-agent

- [ ] Layout loaders for `station` (`kind` string key, `cell`, `footprint`, `worked_sides`; the entity name is the
      placement name), `counter` (1×1 `cell`), `wall` runs with a `height` class (`full`, `low`), an `edge_block` for
      the open south edge (it collides but has no mesh), `prop` (`kind`, `cell`, `footprint`, `solid`; wall-mounted
      props have no footprint) and named `spawn_marker`s.
- [ ] The Shift file lists `in_service` placement names. Every Station it doesn't list is Tarped: it is solid, shows the
      tarp mesh over its footprint, and has no lamp or state.
- [ ] Station, prop and Item kinds are string keys into registries, not enums.
- [ ] A greybox catalogue (data) lists, per kind, primitive parts (cube, sphere, capsule) with offset, size and palette
      token. Named parts (`body`, `lamp`, `riser_bar`, `reel_l`, `reel_r`, `crank`, `platen`, `paper`, `antenna`)
      become named child entities. Silhouettes follow ticket 15: Radio, Decoder, Telex, Keypunch, Counter, tarp.
- [ ] A palette file holds every colour as named tokens: P1 brick red, P2 mustard, the lamp colours, frequency colours
      (7.0 cyan, 7.5 magenta, 8.0 violet), tarp khaki, rope, wood, counter top, walls, floor and rug. Radio bodies use
      a neutral token for now.
- [ ] Lamps and riser bars sit on the top deck or a rear riser, never on the front face. They show idle (lamp off, bar
      empty).
- [ ] The Layout's solid cells form an occupancy grid that is fed to the engine `GridNavGraph`.
- [ ] Load errors name the file and the bad entry: an unknown placement name in `in_service`, a footprint off the
      grid, overlapping solid cells, an unknown kind key.
- [ ] The Room panel gets overlay toggles for the floor grid, solid cells and worked sides.
- [ ] Harness tests: Shift 1 puts Radio1, Decoder and Telex in service and Tarps Radio2, Radio3 and Keypunch. The storm
      window cells (6–7,0) and the gap between Radio2 and Radio3 are not solid. Each bad-data case above fails with
      the expected message.
- [ ] Runtime check: in the running game, the whole floor, north wall and window fit the fixed camera. Components are
      queryable through the Flecs Remote API.
