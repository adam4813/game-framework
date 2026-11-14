# World-object heights stored in tile units

All `WorldObject::height`, `TreeSpecies::minHeight/maxHeight`, and per-catalog `height` fields
store values in **tile units** — not world units. The mesh builder (`BuildUprightObjectMesh`)
multiplies by `tileSize` (0.5 f, one tile in world units) when placing geometry vertices, so a
tree with `height = 2.0` stands exactly two grid cells tall in the scene.

## Why not world units

Storing heights in world units produced non-obvious catalog values. A birch tree at `1.8 f` and
a fir at `5.4 f` do not immediately communicate their visual scale relative to the grid. Using
tile units makes the catalog self-documenting: a deer at `1.6` is slightly shorter than two grid
cells; a hall at `5.0` is five tiles tall; a tree capped at `2.0` never obscures more than two
tile-rows of the view.

## Consequences

- Existing catalog defaults were rescaled on adoption: oak/birch cap at 2 tiles; pine at 3; fir
  at 4. Buildings (house 3, shop 3, barn 4, hall 5) and creatures (cow 1.5, deer 1.6, …) were
  updated accordingly.
- Any future `WorldObject` height set in code (bridge deck, hub markers, well) must use tile
  units. The constant `kBridgeDeckHeight` and town-layout well height are in tiles.
- `UprightMeshConfig::tileSize` must always match the scene's actual tile world size (default
  0.5 f); changing it re-scales all upright geometry uniformly.
