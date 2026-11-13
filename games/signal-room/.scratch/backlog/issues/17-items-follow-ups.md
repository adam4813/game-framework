# Follow-ups from the ticket 05 review (Items)

Status: needs-triage
Source: review of [05: Items: pick up, carry & set down](../../room-bootstrap/issues/05-items-pickup-setdown.md), 2026-10-18
Base branch: `feat/signal-room` (branch off it; land per `games/signal-room/AGENTS.md`)

Known limitations the ticket 05 review raised and the ticket left alone. Each is independent.

- [ ] **Two Operators pressing at one Counter in the same frame both take its Item.** `ItemInteraction` reads
      `(Holds, *)` and `(Carries, *)` before the frame's pair commands merge, so the second Operator still sees the
      Item on the Counter. Two holders break "An Item always has exactly one holder". It can't happen with Shift 1's
      one-Operator Roster, but a two-entry Roster already reaches it (one keyboard drives every Operator). Fix before
      spec 3 seats a second Operator: resolve interactions in one `run()` pass that tracks the Items it moved this
      frame, or claim the Item immediately. Add a harness test with a two-Operator fixture Shift.
- [ ] **`carry_speed` from data is untested.** `AnItemWithCarrySpeedHalfHalvesTheWalkDistance` sets
      `Item::carrySpeed` on the live entity, so the copy from `items.json` (and its 1.0 default) is never checked. The
      Item catalogue path is fixed (`kItemCatalogueFile`). Either let a Shift or the harness point at a fixture
      catalogue, or add a second catalogue Item with `carry_speed` 0.5 and a fixture Shift that places it.
- [ ] **`Item` copies its `ItemKind`.** `SpawnCounterItems` copies `carrySpeed` and `tiltToCamera` onto the entity
      (CODING_STANDARDS.md, "Relationships, not stored copies"). A Reload refreshes them, so it is harmless today.
      Decide whether `Item` should hold only its `id` and read the kind from `ItemCatalogue`, or keep the copy for
      Explorer edits (the current test relies on editing it).
- [ ] **The dust burst writes `ParticleEmitter::accumulator`.** `PuffDust` sets the emitter's spawn debt to the
      particle count with `rate` 0, which avoided an engine change but relies on the emitter's internals. If a second
      game or feature wants one-shot bursts, add a `burst` count to the engine's `ParticleEmitter` (framework commit:
      reflection, level loader, scene-equivalence coverage) and switch to it.
- [ ] **Carry pose constants.** `kCarryAhead`, `kCarryHeight` and `kCarryTiltToCamera` (`room_constants.hpp`) are code;
      the tilt assumes the Layout camera's ~55° pitch. Move them to the greybox catalogue (per Item shape, or an
      `operator` carry anchor) if designers need to tune them, or derive the tilt from the Layout camera.
