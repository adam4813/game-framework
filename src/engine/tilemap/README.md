# Tilemap Module

Data-driven 2D grid system with tile descriptors, a registry, texture-atlas meshing, and an event callback system that
fires C++ or AngelScript functions when entities enter, occupy, or leave tiles.

> **TODO (revisit — likely an API change):** Two related rough edges around tile visuals:
> 1. Interactive/marker tiles (e.g. `teleporter` id 100, `prompt` id 101) are defined as ordinary
>    entries in the *terrain* tileset and use zero-area `tex_coords` so their `color` tint dominates.
>    This conflates "terrain art" with "behavioural markers" and relies on a fake tex rect.
> 2. Tile visuals (`color`/`tex_coords`) are **baked into the mesh once** when `Tilemap` is set at
>    load. Changing a descriptor at runtime (e.g. a script's `td.r/g/b/a`, or gameplay flashing a
>    tile) does **not** re-bake, so visual overrides don't show — only live fields read every frame
>    (`walkable`, the `onenter`/`within`/`onleave` callbacks) take effect. A future API should either
>    mark the tilemap dirty on descriptor change (re-bake) or separate marker/overlay rendering from
>    the baked terrain mesh.

## Components

| Component           | Description                                                                                                              |
|---------------------|--------------------------------------------------------------------------------------------------------------------------|
| `TileDescriptor`    | Data for one tile type: id, name, color, texture rect, walkable flag, `onenter`/`within`/`onleave` callbacks             |
| `TileRegistry`      | Maps tile IDs → descriptors. Stored as a component **on the tilemap entity** (scoped per tilemap, not a world singleton) |
| `TileSet`           | Tileset texture path and pixel dimensions attached to the tilemap entity                                                 |
| `Tilemap`           | Grid of tile IDs: `tile_ids[z * width + x]`                                                                              |
| `GridPosition`      | Entity's current tile column (`x`) and row (`z`)                                                                         |
| `TileCallbackState` | Internal: last-seen tile coordinates for transition detection                                                            |

The registry lives on the same entity as `Tilemap`/`TileSet`, so systems query it alongside the tilemap
(`world.query<const Tilemap, const TileRegistry>()`) and scripts reach it from their host entity
(`self.GetTileRegistry()`).

## Systems

### TilemapMeshBuilder

Two observers on the tilemap entity. When `Tilemap` is set/changed, it bakes the grid into CPU mesh data (positions,
indices, per-vertex tint, UVs from the tileset image) using the entity's scoped
`TileRegistry`, then uploads it as a `render::DynamicMesh`. It only builds geometry — the render module draws it —
keeping tilemap state isolated from rendering.

### TileCallbackSystem (PostUpdate)

Detects tile transitions for entities with `GridPosition`. Per entity per frame:

1. Looks up the current tile descriptor from the tilemap's registry.
2. If the entity moved to a different tile, fires `onleave` on the previous descriptor and `onenter`
   on the new one.
3. Fires `within` every frame while the entity remains on the same tile.

## Loading a tilemap

A tilemap is built from data in two steps, so the caller can inject behaviour into the registry *before* the components
are set — scene `Load()` runs in Flecs **deferred** mode (see
[scenes.md](../../../docs/scenes.md)), so you cannot `get_mut<>` a component you just `set<>`:

```cpp
// 1. Load the tileset: tile visuals + registry + name→id lookup.
auto tileset = engine::tilemap::LoadTileset(world, assetDir + "/data/tilesets/basic.json");

// 2. Inject C++ tile behaviour into the caller-owned registry BEFORE it goes on the entity.
tileset->registry.tiles[100].onenter = [](flecs::entity e, int tx, int tz) { /* teleport */ };

// 3. Build the grid from the map file (ops + brushes), resolving tile names via the tileset.
auto tilemap = engine::tilemap::BuildTilemap(assetDir + "/data/maps/overworld.json", *tileset);

// 4. Assemble the entity — TileSet + TileRegistry before Tilemap so the mesh observer sees them.
world.entity("Tilemap").child_of(sceneRoot)
    .set<engine::tilemap::TileSet>(tileset->info)
    .set<engine::tilemap::TileRegistry>(tileset->registry)
    .set<engine::tilemap::Tilemap>(*tilemap);
```

The **map file** (`assets/data/maps/*.json`) declares `width`/`height`, `tile_size_px`,
`pixels_per_world_unit`, a default `fill` tile, reusable `brushes`, and an `ops` list of grid primitives — `rect`,
`hline`, `vline`, `set`, and `stamp` (expand a named brush at an offset). Tiles are referenced by name (resolved via the
tileset) or numeric id. The **tileset file**
(`assets/data/tilesets/*.json`) declares the texture, atlas dimensions, and per-tile
`id`/`name`/`color`/`tex_coords`/`walkable`. The reusable grid primitives (`SetTile`/`FillRect`/
`HLine`/`VLine`) live in `tilemap_components.hpp` and can also mutate a `Tilemap` at runtime (re-set /
`modified<Tilemap>()` afterwards to re-bake the mesh).

## Script integration

### Authoring tiles from script

`TileDescriptor` is exposed to scripts as a **non-owning reference view** into the registry-owned C++ descriptor.
Scripts fetch the tilemap's registry from their host entity, ask it to create a descriptor, then set fields and the
`OnEnter` callback directly on the returned handle:

```angelscript
void OnInit(Entity self) {
    TileRegistry@ registry = self.GetTileRegistry();   // scoped to the parent tilemap
    TileDescriptor@ td = registry.CreateTile(101);      // create/return a descriptor handle
    td.walkable = true;
    td.r = 0.0f;  td.g = 1.0f;  td.b = 1.0f;  td.a = 1.0f;  // cyan tint

    td.SetOnEnter(@OnPromptTileEnter);                  // wrap the script fn into the descriptor
}

void OnPromptTileEnter(Entity player, int tile_x, int tile_z) {
    Print("Entered tile at (" + tile_x + ", " + tile_z + ")");
    PauseScene();
}
```

Because the view mutates registry-owned storage, `SetOnEnter` wraps the script function straight into the descriptor's
C++ `std::function` (`onenter`) — there is no separate proxy type or stored funcdef handle. `SetOnEnter(null)` clears
the callback. `td.OnEnter(player, x, z)` invokes the stored callback directly (whether it was registered from script or
C++).

> **Why a method, not `td.onEnter = @fn`?** AngelScript property setters coerce their value
> parameter to `const T &in`, which is invalid for funcdef handles, so property-assignment syntax
> cannot accept a callback. A `Set<Name>(@fn)` method is the portable form. Funcdef handles do work
> as ordinary method/function arguments (see `SetOnClick` in the UI module).

### TileRegistry / TileDescriptor script API

| Member                                             | Description                                                                   |
|----------------------------------------------------|-------------------------------------------------------------------------------|
| `Entity.GetTileRegistry()`                         | Get the host/parent tilemap's `TileRegistry@`                                 |
| `TileRegistry.CreateTile(uint id)`                 | Create (or fetch) a descriptor for `id`, return a `TileDescriptor@` view      |
| `TileDescriptor.walkable`, `.r`/`.g`/`.b`/`.a`     | Writable descriptor fields                                                    |
| `TileDescriptor.SetOnEnter(TileEnterCallback@ fn)` | Set the onEnter callback (`funcdef void TileEnterCallback(Entity, int, int)`) |
| `TileDescriptor.OnEnter(Entity, int, int)`         | Invoke the stored onEnter callback directly                                   |

### Global script functions

| Function        | Description                          |
|-----------------|--------------------------------------|
| `PauseScene()`  | Add the `scene::Paused` world tag    |
| `ResumeScene()` | Remove the `scene::Paused` world tag |

### Callback lifecycle

`SetOnEnter` wraps the script function in a reference-counted `ScriptFunctionHandle` stored inside the descriptor's
`std::function`. It is released naturally when the `TileRegistry` is destroyed (e.g. on scene unload, while the engine
is alive). A shutdown callback additionally clears every registry's tile callbacks just before the AngelScript engine is
released, so the final world teardown never releases a script function through a dead engine (the registry destructor
alone cannot guarantee that ordering).

## File layout

```
tilemap_module.hpp / .cpp       Module entry point; component + system + script registration
tilemap_components.hpp          All component structs (plain data)
tilemap_callbacks.hpp / .cpp    TileCallbackSystem
tilemap_mesh_builder.hpp / .cpp TilemapMeshBuilder (bakes the grid into a DynamicMesh)
tilemap.hpp                     Public convenience header
README.md                       This file
```
