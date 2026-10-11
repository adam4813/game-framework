// Tilemap dialog demo: builds a 2-node dialog graph at runtime, registers tile 102 as the
// trigger, and uses ShowDialog / CloseDialog to drive navigation.
// Attached as a child of the Tilemap entity so GetTileSet() resolves correctly.
//
// Dialog choices are configured via the DialogNode component handle (returned by AddDialogNode/
// GetDialogNode). Callbacks receive the clicked button Entity:
//   DialogNode@ dn = entity.GetDialogNode();
//   dn.choice_0_label = "Label";
//   dn.SetOn_choice_0(@OnChoice);   // NodeCallback1 = void(Entity e)

Entity g_greeting;
Entity g_responseRuins;

void OnInit(Entity self) {
    // Create dialog nodes — each entity gets a DialogNode + GraphNode component.
    g_greeting      = CreateDialogNode("ElderGreeting", "The Elder: 'Welcome, traveler. What do you seek?'");
    g_responseRuins = CreateDialogNode("ElderResponseRuins","The Elder: 'The ruins to the north hold great power...and great danger. Tread carefully.'");

    // Configure greeting node choices.
    DialogNode@ greeting = g_greeting.GetDialogNode();
    greeting.choice_0_label = "Tell me about the ancient ruins.";
    greeting.choice_1_label = "Farewell.";
    greeting.SetOn_choice_0(@OnChoiceRuins);
    greeting.SetOn_choice_1(@OnChoiceFarewell);

    // Configure response node.
    DialogNode@ response = g_responseRuins.GetDialogNode();
    response.choice_0_label = "Thank you, Elder.";
    response.SetOn_choice_0(@OnCloseDialog);

    // Structural edges for Flecs Explorer visualization.
    ConnectDialogNodes(g_greeting, g_responseRuins);

    // Register tile 102 as the dialog trigger.
    TileSet@ tileSet = self.GetTileSet();
    TileDescriptor@ td = tileSet.CreateTile(102);
    td.walkable = true;
    td.SetOnEnter(@OnDialogTileEnter);

    Print("[TilemapDialog] Elder dialog ready — walk south of spawn to trigger");
}

void OnDialogTileEnter(Entity entity, int tx, int tz) {
    if (!entity.HasPlayerControlled()) { return; }
    ShowDialog(g_greeting);
}

// Callbacks receive the clicked button Entity (e).
void OnChoiceRuins(Entity e) {
    ShowDialog(g_responseRuins);
}

void OnChoiceFarewell(Entity e) {
    CloseDialog();
}

void OnCloseDialog(Entity e) {
    CloseDialog();
}
