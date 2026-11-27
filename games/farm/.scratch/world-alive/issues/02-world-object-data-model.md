# 02: World-object data model

**What to build:** All the detail added by later tickets — buildings, trees, props, bridge
decks, hub markers, decorative fields, creatures — needs a single, shared representation in
`WorldData` so generation is one pure function, unit-testable without ECS or Platform. This
ticket establishes that foundation: the **world-object layer**.

A world-object is a placed thing that sits on top of the flat terrain tile: `{ footprint
tile(s), kind (data-driven string key), visual height in world-units, blocksMovement }`. The
terrain tile stays flat ground; world-objects provide visual height and optional collision.
`WorldData` gains a typed world-object collection and walkability checking respects world-object
footprints so a one-tile-footprint tree correctly blocks that tile.

Later tickets (04 towns, 05 forests, etc.) populate this layer with real objects; this ticket
adds the data structure and the minimal stub stage that exercises it. The JSON object-kind
registry shape is also established here so every later ticket can extend it without revisiting
the schema.

**Blocked by:** None (can start immediately)

**Status:** done

- [ ] `WorldData` contains a `WorldObject` collection where each entry carries: footprint tile
  coordinates, a kind key (string, resolved against a data-driven registry), visual height, and
  a `blocksMovement` flag.
- [ ] Two calls to `WorldGen::Generate` with the same seed produce identical `WorldObject`
  collections (determinism).
- [ ] The tilemap builder's walkability pass respects world-object footprints: a tile occupied
  by a blocking world-object is treated as non-walkable regardless of the underlying terrain
  tile's own walkability setting.
- [ ] A minimal JSON object-kind registry schema is established (kind key, height, sprite
  reference); later tickets extend it with real entries.
- [ ] Unit tests: the `WorldObject` type exists in `WorldData`; the walkability layer blocks a
  tile covered by a blocking object; determinism holds across two generations.

## Comments
Integration branch: integration/farm-world-alive (tip ffa3a71). Key commit: 434dd29 (world-object data model). Files: world_data.hpp (WorldObject + WorldData::objects), world_gen.cpp (stub stage), farm_tilemap_builder.{hpp,cpp} (Pass 4 footprints), tilemap_components.hpp (blocked_override + IsTileWalkable), grid_movement.cpp, object_kind_registry.{hpp,cpp} (new), assets/data/object_kinds.json (new), world_object_test.cpp (new). 188/188 tests pass.