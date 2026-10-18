#pragma once

namespace engine::ecs {

// Engine-defined pipeline phase: runs after all flecs::OnStore systems complete.
// Use this phase (via `.kind<ecs::OnUI>()`) for 2D overlays, HUD, buttons, and
// debug panels that must composite on top of 3D geometry. UI *rendering* (building and flushing
// the 2D draw list) runs here; UI *layout* runs in PreUpdate and UI *interaction* in OnUpdate,
// like any other simulation, so input is consumed before InputResetFrameState clears it.
struct OnUI {};

// Engine-defined pipeline phase: runs after OnUI. The engine renders the debug UI
// (platform::DebugUi) here, on top of the scene and the 2D UI. Panels themselves don't need this
// phase — they can call widgets from any phase up to OnUpdate (see DebugUi).
struct OnDebugUI {};

} // namespace engine::ecs
