#pragma once

#include <flecs.h>

// The title's decoration: a snapping orange claw at each end of "Crabnado" and little hermit shells perched on its
// letters, drawn into the UI draw list over the outlined title label.
namespace crabnado {

// Marks the title label the claws and shells frame. `top` is the label's top edge in pixels, `font_size` its size,
// and `time` the seconds since the title loaded, which drives the snapping and bobbing.
struct TitleLogo {
	float top{0.0F};
	float font_size{0.0F};
	float time{0.0F};
};

// Registers and reflects TitleLogo and the TitleLogoDraw system. Call from the module constructor.
void RegisterTitleLogo(const flecs::world& world);

} // namespace crabnado
