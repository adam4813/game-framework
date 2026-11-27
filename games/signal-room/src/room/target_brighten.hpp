#pragma once

#include <flecs.h>

namespace signal_room {

// Register the Target brighten as children of the Room's root entity, in PreStore so they see this
// frame's Targets: TargetBrightenRestore puts back the PartColour of every Brightened placement no
// Operator targets any more, then TargetBrighten blends each Target's parts toward its Operator's
// identity colour (lightened by brightenLift) by up to brightenStrength, pulsing at brightenRate. No
// other entity changes colour.
void RegisterTargetBrighten(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
