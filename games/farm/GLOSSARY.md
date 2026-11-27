# Farm

A data-driven farming and exploration game on a procedurally generated World. Builds on the engine vocabulary
in `docs/GLOSSARY.md`.

## Language

**Farm**:
The farm Game Target built from `games/farm/` — a data-driven farming and exploration game with a procedurally generated World. Cf. Sandbox.
_Avoid_: farm demo, farm prototype

**World**:
The procedurally generated state of a farm — terrain, water, towns, roads, placed objects, and creatures — produced by world generation from a Seed and generation parameters. A World is one state with two lenses: the World Preview renders it as a biome map, the in-game Farm builds it into a tilemap and entities. Both read the same World.
_Avoid_: map, level, terrain, WorldData (the type name; the concept is the World)

**World Preview**:
The pre-game lens that renders a World as a one-pixel-per-tile biome map so the player can inspect the world their Seed will produce before committing to play. It generates World data only; it never builds the gameplay tilemap or spawns entities.
_Avoid_: preview scene, worldgen preview, map preview

**Seed**:
The integer that, together with the generation parameters and catalogs, deterministically produces a World. Rerolling the Seed in the World Preview regenerates the World; the chosen Seed is persisted with the Save Slot.
_Avoid_: world seed, rng seed

**Save Slot**:
One of a fixed set of named persistence slots (`farm_slot_1`..`farm_slot_3`) holding a saved Farm. A slot stores the Seed, generation parameters, and the player's mutations (position, game time, tilled soil and crops); loading a slot reconstructs the World by regenerating from the Seed and parameters and overlaying the mutations.
_Avoid_: save file, save game, profile

**Tile**:
The fundamental unit of space in the Farm world: a single grid cell on the terrain, used as the base unit of movement, placement, and height measurement. One tile equals `tileSize = 0.5` world units in the 3D scene. All `WorldObject` heights (trees, buildings, creatures), road clearances, and town radii are expressed in tiles; the mesh builder scales tile values to world units via `height × tileSize`.
_Avoid_: cell, grid square, texel

**Load Region**:
A rectangular sub-area of the World whose tilemap tiles and ECS entities are built and spawned together. After world generation produces a complete `WorldData`, Load Regions phase in progressively: the region enclosing the player's spawn point is built first, then expanding bands outward. AI systems require their entities to be fully spawned in a Load Region before they can activate for that region. The navigation grid (walkable/blocked per tile) is pre-built from the full World before any Load Region is constructed.
_Avoid_: chunk (overloaded with render-mesh batches in `FarmUprightObjectStreaming`), sector, zone

**Occluder**:
An upright object (tree, building) taller than one Tile that stands between the camera and the player and covers the player on screen. An Occluder is drawn see-through as a whole, by the engine's translucent pass, so the player stays visible behind it; it turns opaque again once the player steps clear. Villagers, creatures and objects one Tile tall or less never become Occluders.
_Avoid_: occluder fade circle, cutout hole, ghost
