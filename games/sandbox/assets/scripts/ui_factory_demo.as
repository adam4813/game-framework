// ui_factory_demo.as — Demonstrates the engine::ui Create* factory globals exposed to scripting
// (Phase 7): a script building its own UI at runtime, rather than only reading existing elements.
// Each Create* takes a parent Entity (the new widget is parented to it) plus the screen-space rect
// and widget-specific arguments, and returns the created Entity so children can be nested under it.
//
// Widgets are parented to `self` (the host entity), which the title scene gives a full-screen UIRect
// and parents under the title UI root. Parenting to `self` (always valid) rather than
// self.GetParent() keeps the whole demo subtree owned by the host, so it is destroyed with the title
// scene — no cross-scene leak — and still renders in absolute screen coordinates.

void OnInit(Entity self) {
    // A panel container, then a label + button nested inside it.
    Entity panel = CreatePanel(self, 250.0f, 20.0f, 250.0f, 120.0f);
    CreateLabel(panel, 266.0f, 30.0f, 220.0f, 24.0f, "Built from script", 18.0f);

    Entity button = CreateButton(panel, 266.0f, 66.0f, 160.0f, 40.0f, "Click me");
    SetOnClick(button, @OnFactoryButtonClicked);
}

void OnFactoryButtonClicked(Entity button) {
    Print("[ui_factory_demo] clicked button '" + button.GetName() + "'");
}

void Tick(Entity self, float dt) {}
void OnDestroy(Entity self) {}
