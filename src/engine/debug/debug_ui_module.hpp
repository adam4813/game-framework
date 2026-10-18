#pragma once

#include <flecs.h>

namespace engine::debug {

// Hosts the debug UI (platform::DebugUi) in the pipeline:
//   - registers the ecs::OnDebugUI phase (after ecs::OnUI)
//   - DebugUiDraw         (OnDebugUI) — renders this frame's debug UI on top of everything
//   - EngineDebugPanel    (OnUpdate)  — built-in "Engine" panel: FPS, frame time, entity count
//
// Game panels are plain systems that call Platform::GetDebugUi() widgets, usually in OnUpdate.
class DebugUiModule {
public:
	explicit DebugUiModule(const flecs::world& world);
};

} // namespace engine::debug
