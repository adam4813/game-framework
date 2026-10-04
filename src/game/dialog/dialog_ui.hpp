#pragma once

#include <flecs.h>

namespace game::dialog {

/// UI-runtime singleton: the persistent full-screen root the dialog box is parented to,
/// and the currently-shown box (destroyed on teardown/rebuild).
struct DialogUI {
	flecs::entity root;
	flecs::entity box;
};

/// (Re)build the on-screen dialog box for the active interaction's current node: portrait,
/// speaker, wrapped text, and option/continue buttons. No-op when no interaction is active.
void BuildDialogBox(const flecs::world& world);

/// Destroy the on-screen dialog box, if any.
void DestroyDialogBox(const flecs::world& world);

} // namespace game::dialog
