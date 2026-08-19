// ui_demo.as — Drives a UI ProgressBar from script, proving scripts can read and write engine UI
// components live (the UI-layer analogue of sun_cycle.as mutating a render component).
//
// Attached as a child ScriptComponent of the "DemoProgress" entity in the title scene, so `self`
// is the progress-bar entity. self.GetProgressBar() returns a handle straight into ECS storage;
// writing bar.value is picked up by the UI render pass the same frame.

float g_t = 0.0f;

void OnInit(Entity self) {
    // Component getters resolve to the host (this script's parent), so GetParent() names it.
    Print("[ui_demo] Driving ProgressBar on '" + self.GetParent().GetName() + "'");
}

void Tick(Entity self, float dt) {
    ProgressBar@ bar = self.GetProgressBar();

    // Loop the fill from 0 to 1 over five seconds.
    g_t += dt * 0.2f;
    if (g_t > 1.0f) {
        g_t -= 1.0f;
    }
    bar.value = g_t;
}

void OnDestroy(Entity self) {}
