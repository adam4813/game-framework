// Tilemap dialog demo: builds the Elder conversation from script with the engine dialog module's
// API, and starts it when the player steps on tile 102 (north of spawn).
// Attached as a child of the Tilemap entity, so GetTileSet() resolves and the tree (parented to the
// host) is destroyed with the scene.
//
// Flow: the tree's root is the first node shown. A choice is a child node carrying SetDialogChoice;
// picking it runs that node. A node marked SetDialogEnd closes the conversation.
//
//   ElderDialog  "Welcome, traveler. What do you seek?"
//   ├─ AskAboutRuins  [Tell me about the ancient ruins.]  "The ruins to the north..."
//   │  └─ ThankElder  [Thank you, Elder.]  (end)
//   └─ Farewell       [Farewell.]  (end)

Entity g_elder;

void OnInit(Entity self) {
    g_elder = CreateDialogTree("ElderDialog", self);
    SetDialogLine(g_elder, "The Elder", "Welcome, traveler. What do you seek?", "");

    Entity ruins = AddDialogNode(g_elder, "AskAboutRuins");
    SetDialogChoice(ruins, "Tell me about the ancient ruins.", 0, "");
    SetDialogLine(ruins, "The Elder", "The ruins to the north hold great power...and great danger. Tread carefully.", "");
    ConnectDialogNodes(g_elder, ruins);

    Entity thanks = AddDialogNode(g_elder, "ThankElder");
    SetDialogChoice(thanks, "Thank you, Elder.", 0, "");
    SetDialogEnd(thanks);
    ConnectDialogNodes(ruins, thanks);

    Entity farewell = AddDialogNode(g_elder, "Farewell");
    SetDialogChoice(farewell, "Farewell.", 1, "");
    SetDialogEnd(farewell);
    ConnectDialogNodes(g_elder, farewell);

    // Tile 102 is the trigger.
    TileSet@ tileSet = self.GetTileSet();
    TileDescriptor@ td = tileSet.CreateTile(102);
    td.walkable = true;
    td.SetOnEnter(@OnDialogTileEnter);

    Print("[TilemapDialog] Elder dialog ready - walk north of spawn to trigger");
}

void OnDialogTileEnter(Entity entity, int tx, int tz) {
    if (!entity.HasPlayerControlled()) { return; }
    StartDialog(g_elder, entity);
}
