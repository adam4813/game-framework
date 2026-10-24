#pragma once

#include <flecs.h>

// The Settings modal, shared by the title screen and the pause menu.
namespace crabnado {

// Builds the Settings modal, closed, as a child of `parent` and centred on a `width` x `height` screen: a volume slider
// and a Mute box for the music and for the sound effects, showing the current AudioSettings, a Fullscreen box, and a
// Back button that closes it. Each change applies and saves at once. Open it with `get_mut<ui::Modal>().open = true`.
flecs::entity BuildSettingsModal(const flecs::world& world, flecs::entity parent, float width, float height);

} // namespace crabnado
