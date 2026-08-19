#pragma once

#include <flecs.h>

#include "engine/engine.hpp"

namespace game {

// Builds a small, demonstrative save-management panel at `rect`, parented to `parent`:
//   * a "New Save" button (writes a new auto-named slot),
//   * a scrollable list of existing slots, each with a Load and a Delete button.
// The list rebuilds itself live whenever slots change. Returns the panel entity (destroyed with the
// parent). Backed by ListSaveSlots / SaveGameToSlot / LoadGameFromSlot / DeleteSaveSlot, which in
// turn use the Platform save API (desktop files / browser localStorage).
flecs::entity
BuildSaveBrowser(const flecs::world& world, const engine::platform::Rect& rect, const flecs::entity& parent);

} // namespace game
