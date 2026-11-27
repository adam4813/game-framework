# 02: Shift 1 Layout & greybox reads

Spec: [Signal Room spec 1: Room & bootstrap](../spec.md). Read: [Shift & Layout data](../spec/shift-and-layout.md), [Camera & greybox reads](../spec/greybox-and-camera.md), [Debugging & tuning](../spec/debugging.md), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (this game's main: branch off it, PR back into it, never into `main`)

**What to build:** Launching the game shows Shift 1's 13×9 room as designed in
[Room layout, movement & set-down](../../wayfinder/issues/07-room-layout-movement.md), including its comments
(three Radio placements, the Counter at (0,8) replacing the Vent, and which tarps are which machines). In-service
Stations (Radio1, Decoder, Telex) stand as greybox machines on tables with idle lamps and risers. Every other Station
(Radio2, Radio3, Keypunch) stands Tarped. Walls, the rug, the storm window, the foreground desk edge and the dressing
tarps are in place. A designer can move a machine, list another Station as in service, or recolour something in JSON
and see it after Reload. Shapes and colours follow
[Greybox readability & juice](../../wayfinder/issues/15-greybox-readability.md).

**Blocked by:** 01 (Game Target & headless Room harness).

**Status:** done

- [x] Layout loaders for `station` (`kind` string key, `cell`, `footprint`, `worked_sides`; the entity name is the
      placement name), `counter` (1×1 `cell`), `wall` runs with a `height` class (`full`, `low`), an `edge_block` for
      the open south edge (it collides but has no mesh), `prop` (`kind`, `cell`, `footprint`, `solid`; wall-mounted
      props have no footprint) and named `spawn_marker`s.
- [x] The Shift file lists `in_service` placement names. Every Station it doesn't list is Tarped: it is solid, shows the
      tarp mesh over its footprint, and has no lamp or state.
- [x] Station, prop and Item kinds are string keys into registries, not enums.
- [x] A greybox catalogue (data) lists, per kind, primitive parts (cube, sphere, capsule) with offset, size and palette
      token. Named parts (`body`, `lamp`, `riser_bar`, `reel_l`, `reel_r`, `crank`, `platen`, `paper`, `antenna`)
      become named child entities. Silhouettes follow ticket 15: Radio, Decoder, Telex, Keypunch, Counter, tarp.
- [x] A palette file holds every colour as named tokens: P1 brick red, P2 mustard, the lamp colours, frequency colours
      (7.0 cyan, 7.5 magenta, 8.0 violet), tarp khaki, rope, wood, counter top, walls, floor and rug. Radio bodies use
      a neutral token for now.
- [x] Lamps and riser bars sit on the top deck or a rear riser, never on the front face. They show idle (lamp off, bar
      empty).
- [x] The Layout's solid cells form an occupancy grid that is fed to the engine `GridNavGraph`.
- [x] Load errors name the file and the bad entry: an unknown placement name in `in_service`, a footprint off the
      grid, overlapping solid cells, an unknown kind key.
- [x] The Room panel gets overlay toggles for the floor grid, solid cells and worked sides.
- [x] Harness tests: Shift 1 puts Radio1, Decoder and Telex in service and Tarps Radio2, Radio3 and Keypunch. The storm
      window cells (6–7,0) and the gap between Radio2 and Radio3 are not solid. Each bad-data case above fails with
      the expected message.
- [x] Runtime check: in the running game, the whole floor, north wall and window fit the fixed camera. Components are
      queryable through the Flecs Remote API.

## Comments

**2026-10-18 (implementation):** Done on `feat/sr-room-02-shift1-layout-greybox`.

- Data: `assets/greybox/palette.json` (colour tokens), `assets/greybox/catalogue.json` (per-kind primitive parts,
  wall height classes, floor colours), the Shift 1-2 Layout `layouts/bunker.layout.json` and `in_service` in
  `shifts/shift_01.shift.json`. Each Layout entity carries a `comment` naming its floorplan role.
- Station and prop kinds and wall height classes are string keys into the greybox catalogue. **Item kinds have no
  registry yet:** Items arrive with ticket 05, which should add the Item catalogue the same way.
- Load order in `RoomScene::BuildRoom`: palette, catalogue, Shift, `LoadLayout` (kind/grid/overlap checks, occupancy,
  `GridNavGraph`), `ApplyInService` (Tarps the rest), `BuildLayoutGreybox`, overlays. A failed load empties the Room.
- Catalogue additions beyond the ticket: a part may name an earlier part as its `parent` (the crank grip turns with
  the crank), `visible: false` builds a part disabled (the Telex paper), and `fit: footprint` stretches a model to its
  footprint (tarps, rug, desk edge). A prop may set `height` to scale a fitted model (the 1.8 m Supply Rack tarp).
  Spawn markers on a solid or off-grid cell are a load error.
- Walls and the edge block stand on the ring of cells around the 13x9 grid, so they claim no occupancy cells
  (`FloorGrid::IsSolid` already treats off-grid cells as solid). Ticket 03's collision should read `Wall` and
  `EdgeBlock` placements as AABBs, or simply treat off-grid as blocked.
- The solid-cells overlay floats a flat marker over each cell, shifted along the camera's view so it projects onto its
  cell. Wireframe was tried first, but the instanced path showed nothing and WebGL has no wire mode.
- Tests went from 26 to 40 (`room_layout_test`, 14 tests).
- Runtime check: on desktop the whole floor, north wall and window fit the camera. Station, Tarped and RoomOverlays
  are queryable over REST (`worked_sides` shows as bit names), and the overlays draw. The WASM build loads in Chrome.
