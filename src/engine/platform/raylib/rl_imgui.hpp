#pragma once

#include <span>

// Minimal Raylib <-> Dear ImGui bridge (vendored from the blacksmith prototype's rlImGui, adapted).
// Used only by RaylibBackend; requires a live ImGui context (ImGuiDebugUi) and an open window.
// Neither raylib nor ImGui types appear here so the header stays backend-agnostic.
namespace engine::platform::rlimgui {

// Uploads the ImGui font atlas as a Raylib texture. Call once after InitWindow.
void Setup();
void Shutdown();

// Queues this frame's mouse, keyboard and typed characters on ImGui's IO. Key and button states
// are sent as levels (ImGui drops unchanged ones), so state resyncs after the UI was hidden.
void QueueInput(std::span<const int> chars);

// Draws the draw data from the last ImGui::Render with rlgl, on top of whatever is in the batch.
void RenderDrawData();

} // namespace engine::platform::rlimgui
