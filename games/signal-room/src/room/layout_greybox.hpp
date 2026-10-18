#pragma once

#include <flecs.h>

namespace signal_room {

// Build the greybox look of a loaded Layout under `parent` from the GreyboxCatalogue singleton: the
// floor tiles, the walls, each Station (its machine parts in service, the tarp when Tarped), each
// Counter (with an idle `dust` particle emitter on its top for set-downs) and each prop, each through
// BuildGreyboxModel. Named catalogue parts become named children of their placement. Call after LoadLayout and ApplyInService.
void BuildLayoutGreybox(const flecs::world& world, flecs::entity parent);

} // namespace signal_room
