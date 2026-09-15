# Tilemap Module

Data-driven 2D grid system with tile descriptors, a gameplay `TileSet` registry, a render-module `SpriteAtlas`,
texture-atlas meshing, and an event callback system that fires C++ or AngelScript functions when
entities enter, occupy, or leave tiles.

> **TODO (revisit — likely an API change):** Two related rough edges around tile visuals:
> 1. Interactive/marker tiles (e.g. `teleporter` id 100, `prompt` id 101) are defined as ordinary
>    entries in the *terrain* tileset and use zero-area `tex_coords` so their `color` tint
>    dominates. This conflates "terrain art" with "behavioural markers" and relies on a fake tex
>    rect.
> 2. Tile visuals (`color`/`tex_coords`) are **baked into the mesh once** when `Tilemap` is set at
>    load. Changing a descriptor at runtime (e.g. a script's `td.r/g/b/a`, or gameplay flashing a
>    tile) does **not** re-bake, so visual overrides don't show — only live fields read every frame
>    (`walkable`, the `onenter`/`within`/`onleave` callbacks) take effect. A future API should
>    either mark the tilemap dirty on descriptor change (re-bake) or separate marker/overlay
>    rendering from the baked terrain mesh.

## Components

| Component           | Description                                                                                                                                        |
|---------------------|----------------------------------------------------------------------------------------------------------------------------------------------------|
| `TileDescriptor`    | Data for one tile type: id, name, color, texture rect, walkable flag, `onenter`/`within`/`onleave` callbacks                                       |
| `TileSet`           | Maps tile IDs → descriptors and tile names → IDs (`name_to_id`). Usually stored on a dedicated registry entity                                     |
| `SpriteAtlas`       | *(From render module)* Sprite atlas texture path and pixel dimensions. Stored on a shared atlas/material entity                                    |
| `UseRegistry`       | Relationship from a tilemap entity to the registry entity carrying `TileSet`                                                                       |
| `UseSpriteAtlas`    | *(From render module)* Relationship from a tilemap entity to the atlas entity carrying `SpriteAtlas`. An observer wires `RenderWith` automatically |
| `TilemapSpec`       | **Instructions** for building a Tilemap: width, height, fill tile, and a flat list of `TilemapOp` painting operations                              |
| `Tilemap`           | **Baked result**: flat `tile_ids[z * width + x]` array. Produced by `TilemapBakeOnSpec` from `TilemapSpec`                                         |
| `TilemapOp`         | A single resolved grid op (`Set`, `FillRect`, `HLine`, `VLine`). Stores resolved `tile_id` or deferred `tile_name`                                 |
| `GridPosition`      | Entity's current tile column (`x`) and row (`z`)                                                                                                   |
| `TileCallbackState` | Internal: last-seen tile coordinates for transition detection                                                                                      |
| `TilemapViewport`   | Optional: viewport-sized ring-buffer configuration for efficient streaming                                                                         |

## Spec / baked-data split

`TilemapSpec` is the *instruction set* and `Tilemap` is the *compiled output*:

- `TilemapSpec` holds all data needed to produce a `Tilemap`: dimensions, fill tile ID, and a flat
  list of `TilemapOp` entries (tile names resolved eagerly when `TileSet` is present; stored as
  `tile_name` when not).
- `TilemapBakeOnSpec` fires on `OnSet<TilemapSpec>` and calls `BakeSpec(spec, tileSet)` to produce
  `Tilemap`. The optional `TileSet*` resolves deferred `tile_name` entries.
- To **re-bake** at runtime (for example after patching `TilemapSpec.ops`): `e.remove<Tilemap>()`
  then `e.modified<TilemapSpec>()`.

Gameplay and rendering data are intentionally split:

- the tilemap resolves tile behaviour through `e.target<level::UseRegistry>().get<TileSet>()`
- the tilemap resolves its material/atlas through `e.target<render::UseSpriteAtlas>()`
- scripts access the gameplay registry through `self.GetTileSet()`

## Viewport Streaming and TilemapFollowTarget

For large tilemaps, enabling viewport-sized rendering avoids baking the entire grid into one mesh:

```cpp
auto registry = world.entity("BasicRegistry").child_of(sceneRoot)
    .set<engine::tilemap::TileSet>(/* ... */);

auto atlas = world.entity("BasicSheet").child_of(sceneRoot)
    .set<engine::render::SpriteAtlas>(/* ... */)
    .set<engine::render::AlbedoMap>(/* ... */)
    .set<engine::render::Material>(/* ... */);

world.entity("Tilemap").child_of(sceneRoot)
    .set<engine::tilemap::TilemapViewport>({.buffer_width = 0, .buffer_height = 0})  // 0 = auto-compute
    .add<engine::level::UseRegistry>(registry)
    .add<engine::render::UseSpriteAtlas>(atlas)
    .set<engine::tilemap::Tilemap>(*tilemap);  // mesh builder sees viewport and creates ring-buffer
```

Mark the entity that the viewport should follow (typically the player) with the **tag**
`TilemapFollowTarget`:

```cpp
player.add<engine::tilemap::TilemapFollowTarget>();
```

The tilemap's streaming system queries this tag and centers the viewport window on the target's
`WorldTransform.position`. If no target exists, it falls back to the map center, so a valid static
view still renders.

## Systems

### TilemapMeshBuilder

Observer that fires on `OnSet<Tilemap>`. Composes the tilemap with its resolved `TileSet` (via `UseRegistry` ref)
and its `SpriteAtlas` (via `UseSpriteAtlas` ref), then bakes the grid into CPU mesh data (positions, indices,
per-vertex tint, UVs from the atlas image) and uploads it as a `render::DynamicMesh`. It only builds geometry — the
render module draws it.

### TileCallbackSystem (PostUpdate)

Detects tile transitions for entities with `GridPosition`. Per entity per frame:

1. Looks up the current tile descriptor from the active tilemap's `TileSet`.
2. If the entity moved to a different tile, fires `onleave` on the previous descriptor and
   `onenter` on the new one.
3. Fires `within` every frame while the entity remains on the same tile.

## Level JSON authoring

The recommended way to author a tilemap is with three entities: a gameplay registry, a render
atlas, and the tilemap itself:

```json
{
  "entities": [
    {
      "name": "BasicRegistry",
      "components": {
        "tileset": "@file:components/basic_tileset.json"
      }
    },
    {
      "name": "BasicSheet",
      "components": {
        "sprite_atlas": {
          "texture_path": "textures/tileset_basic.png",
          "image_width_px": 128,
          "image_height_px": 256
        },
        "shader": {
          "stages": [
            {"type": "vertex", "path": "tilemap.vs"},
            {"type": "fragment", "path": "tilemap.fs"}
          ]
        }
      }
    },
    {
      "name": "Tilemap",
      "components": {
        "tilemap_spec": {"map": "data/maps/overworld.json"}
      },
      "refs": {
        "use_registry": "BasicRegistry",
        "use_sprite_atlas": "BasicSheet"
      }
    }
  ]
}
```

- `"tileset"` sets `TileSet` on the registry entity
- `"sprite_atlas"` sets `SpriteAtlas` on the atlas entity
- `"use_sprite_atlas"` wires `UseSpriteAtlas` relationship from the tilemap to the atlas entity
- `"tilemap_spec"` parses the map JSON into `TilemapSpec`, and the bake observers build `Tilemap`

To load a pre-baked tilemap from an editor (flat `tile_ids` array), keep the same refs and author:

```json
{
  "tilemap": {
    "width": 64,
    "height": 64,
    "tile_size_px": 16,
    "pixels_per_world_unit": 32,
    "tile_ids": []
  }
}
```

For save/load or tooling round-trips, use the matching C++ helpers:

- `engine::tilemap::TilemapToJson(const Tilemap&)` → `{ width, height, tile_size_px, pixels_per_world_unit, tile_ids }`
- `engine::tilemap::TilemapFromJson(const nlohmann::json&)` ← same shape as above (and the `"tilemap"` loader schema)

## C++ workflow

```cpp
auto registry = world.entity("BasicRegistry").child_of(sceneRoot);
auto atlas = world.entity("BasicSheet").child_of(sceneRoot);

// 1. Load gameplay data into the registry entity.
engine::tilemap::LoadTileSetIntoEntity(registry, assetDir + "/data/tilesets/basic.json");

// 2. Load rendering data (SpriteAtlas, AlbedoMap, Material, ShaderMap) into the atlas entity.
atlas.set<engine::render::SpriteAtlas>({/* ... */})
    .set<engine::render::AlbedoMap>({/* ... */})
    .set<engine::render::Material>({/* ... */});

// 3. Parse map ops into a TilemapSpec (names resolved via the loaded TileSet).
const auto& tileSet = registry.get<engine::tilemap::TileSet>();
auto spec = engine::tilemap::ParseTilemapSpec(
    assetDir + "/data/maps/overworld.json", tileSet.name_to_id);

// 4. Inject C++ tile callbacks before baking.
registry.get_mut<engine::tilemap::TileSet>().tiles[100].onenter =
    [](flecs::entity e, int tx, int tz) { /* teleport */ };

// 5. Set the tilemap. TilemapBakeOnSpec fires and bakes Tilemap automatically.
world.entity("Tilemap").child_of(sceneRoot)
    .set<engine::tilemap::TilemapViewport>({})
    .add<engine::level::UseRegistry>(registry)
    .add<engine::render::UseSpriteAtlas>(atlas)
    .set<engine::tilemap::TilemapSpec>(std::move(*spec));
```

The **map file** (`assets/data/maps/*.json`) declares `width`/`height`, `tile_size_px`,
`pixels_per_world_unit`, a default `fill` tile, reusable `brushes`, and an `ops` list of grid
primitives — `rect`, `hline`, `vline`, `set`, and `stamp`. Tiles are referenced by name (resolved via the `TileSet`) or
numeric id. The **tileset file** (`assets/data/tilesets/*.json`) declares the texture path, atlas dimensions, and
per-tile `id`/`name`/`color`/`tex_coords`/`walkable`.

## Script integration

`TileDescriptor` is exposed to scripts as a **non-owning reference view** into the TileSet-owned
C++ descriptor. Scripts fetch the tilemap's `TileSet`, ask it to create a descriptor, then set
fields and the `OnEnter` callback directly on the returned handle:

```angelscript
void OnInit(Entity self) {
    TileSet@ tileSet = self.GetTileSet();
    TileDescriptor@ td = tileSet.CreateTile(101);
    td.walkable = true;
    td.r = 0.0f;
    td.g = 1.0f;
    td.b = 1.0f;
    td.a = 1.0f;

    td.SetOnEnter(@OnPromptTileEnter);
}
```

### TileSet / TileDescriptor script API

| Member                                             | Description                                                              |
|----------------------------------------------------|--------------------------------------------------------------------------|
| `Entity.GetTileSet()`                              | Get the host/parent tilemap's `TileSet@`                                 |
| `TileSet.CreateTile(uint id)`                      | Create (or fetch) a descriptor for `id`, return a `TileDescriptor@` view |
| `TileDescriptor.walkable`, `.r`/`.g`/`.b`/`.a`     | Writable descriptor fields                                               |
| `TileDescriptor.SetOnEnter(TileEnterCallback@ fn)` | Set the onEnter callback                                                 |
| `TileDescriptor.OnEnter(Entity, int, int)`         | Invoke the stored onEnter callback directly                              |

### Callback lifecycle

`SetOnEnter` wraps the script function in a reference-counted `ScriptFunctionHandle` stored inside
the descriptor's `std::function`. It is released naturally when the `TileSet` is destroyed (for
example on scene unload, while the engine is alive). A shutdown callback additionally clears every
registry's tile callbacks just before the AngelScript engine is released.

## File layout

```text
tilemap_module.hpp / .cpp       Module entry point; component + system + script registration
tilemap_components.hpp          All component structs (plain data)
tilemap_callbacks.hpp / .cpp    TileCallbackSystem
tilemap_mesh_builder.hpp / .cpp TilemapMeshBuilder (bakes the grid into a DynamicMesh)
tilemap.hpp                     Public convenience header
README.md                        This file
```
