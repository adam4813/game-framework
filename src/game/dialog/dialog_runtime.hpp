#pragma once

#include <string>

#include <flecs.h>

#include "dialog_components.hpp"

namespace game::dialog {

// ---------------------------------------------------------------------------
// Handler registry
// ---------------------------------------------------------------------------

/// Register (or override) the handler for a node type. Call from C++ or via the
/// scripting binding. Overriding a built-in type (e.g. "give_item") lets a game supply
/// its own effect while reusing the interaction runner.
void RegisterDialogNodeHandler(const flecs::world& world, std::string type, DialogNodeHandler handler);

// ---------------------------------------------------------------------------
// Story flags
// ---------------------------------------------------------------------------

void SetDialogFlag(const flecs::world& world, const std::string& flag, bool value);
[[nodiscard]] bool GetDialogFlag(const flecs::world& world, const std::string& flag);

// ---------------------------------------------------------------------------
// Runtime control
// ---------------------------------------------------------------------------

/// Begin an interaction at `root` for `actor`. Creates a DialogRunner entity, pauses the
/// scene (adds scene::Paused) and steps into the first node. Returns the runner entity.
flecs::entity StartDialog(const flecs::world& world, flecs::entity root, flecs::entity actor);

/// Resume a waiting runner. `chosen` is the selected option edge for a choice/line node;
/// pass an invalid entity to advance along the node's first eligible child. Steps through
/// any instant nodes until the next block or the end.
void ResolveDialog(const flecs::world& world, flecs::entity runner, flecs::entity chosen);

/// Finish an interaction immediately: tears down the UI, unpauses, and destroys the runner.
void EndDialog(const flecs::world& world, flecs::entity runner);

// ---------------------------------------------------------------------------
// Traversal helpers (also used by the UI layer)
// ---------------------------------------------------------------------------

/// First child of `node` (by DialogChoice.order then graph::GraphNode.order) whose gate
/// passes, ignoring branch-only children. Returns an invalid entity when there is none.
[[nodiscard]] flecs::entity FirstEligibleChild(const flecs::world& world, flecs::entity node);

} // namespace game::dialog
