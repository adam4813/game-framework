// Tile callback demo: registers a prompt tile (ID 101) with a script-defined onEnter callback.
// This script is attached as a child of the Tilemap entity, so `self.GetTileRegistry()` resolves
// to the parent tilemap's registry (component accessors resolve to the host/parent entity).
//
// Note: use @FunctionName to pass a function as a funcdef handle. Passing without @ creates an
// implicit delegate wrapper which causes an "external reference" warning on shutdown.

void OnInit(Entity self) {
    Print("[TileCallbackDemo] Registering prompt tile 101");

    // Fetch the tilemap's registry and create a tile descriptor for id 101.
    TileRegistry@ registry = self.GetTileRegistry();
    TileDescriptor@ td = registry.CreateTile(101);
    td.walkable = true;
    // NOTE: colour changes below do NOT currently apply — tile visuals are baked into the mesh from
    // the tileset at load and are not re-baked when a descriptor changes at runtime. The rendered
    // colour comes from the tileset (assets/data/tilesets/basic.json). See the tilemap README TODO.
    td.r = 0.0f;
    td.g = 1.0f;
    td.b = 1.0f;
    td.a = 1.0f; // Cyan tint

    // The callback lives directly on the registry's descriptor.
    td.SetOnEnter(@OnPromptTileEnter);

    Print("[TileCallbackDemo] Prompt tile 101 registered with onEnter callback");
}

// Called by the tile callback system when the player steps onto tile 101.
void OnPromptTileEnter(Entity player, int tile_x, int tile_z) {
    Print("[TileCallbackDemo] Player entered prompt tile at (" + tile_x + ", " + tile_z + ")");
    PauseScene();
}
