// ui_factory_demo.as — Demonstrates the engine::ui Create* factory globals exposed to scripting
// (Phase 7): a script building its own UI at runtime, rather than only reading existing elements.
// Each Create* takes a parent Entity (the new widget is parented to it) plus the screen-space rect
// and widget-specific arguments, and returns the created Entity so children can be nested under it.
//
// Attached to a child of the title UI root, so self.GetParent() is that root and the widgets render
// in absolute screen coordinates.

void OnInit(Entity self) {
    Entity root = self.GetParent();

    // A panel container, then a label + button nested inside it.
    Entity panel = CreatePanel(root, 250.0f, 20.0f, 250.0f, 120.0f);
    CreateLabel(panel, 266.0f, 30.0f, 220.0f, 24.0f, "Built from script", 18.0f);

    Entity button = CreateButton(panel, 266.0f, 66.0f, 160.0f, 40.0f, "Click me");
    SetOnClick(button, @OnFactoryButtonClicked);
}

void OnFactoryButtonClicked(Entity button) {
    Print("[ui_factory_demo] clicked button '" + button.GetName() + "'");
}

void Tick(Entity self, float dt) {}
void OnDestroy(Entity self) {}
