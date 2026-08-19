// Button click demo: binds a script function as the "Script Button" onClick handler.
// This script is attached as a child of the button entity, so self.GetParent() is the button.
//
// SetOnClick wraps the handler into the button's OnClick component (the same storage C++ callers
// use), so the normal UI interaction and UIClickRequest paths fire it. The handler is entity-first
// (script functions have no implicit receiver): the clicked button is passed as the argument.
//
// Note: use @FunctionName to pass a function as a funcdef handle.

void OnInit(Entity self) {
    Print("[ButtonClickDemo] Binding onClick handler to the Script Button");
    SetOnClick(self.GetParent(), @OnScriptButtonClicked);
}

void OnScriptButtonClicked(Entity button) {
    Print("[ButtonClickDemo] Script Button clicked: " + button.GetName());
}
