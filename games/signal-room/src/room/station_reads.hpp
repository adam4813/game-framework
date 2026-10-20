#pragma once

#include <flecs.h>

namespace signal_room {

// Register, as children of the Room's root entity (after the Job and Radio systems, before
// RegisterTargetBrighten):
// - StationReadDrive: derives each Station's StationRead from its Job (and, for a Radio, its online and
//   coupler state) and drives the greybox's named parts from it: `lamp`, `riser_bar`, `body` (a dark
//   Radio's dims), `reel_l` / `reel_r` / `crank` (spin) and `paper` (shown while loaded, slides up on a
//   transmission). Parts a kind lacks are skipped. Colours go through PartColour so the Target brighten
//   still tints over them.
// - StationTransmissionFlash: on a Transmission, starts the transmitting Station's flash and slide and
//   bursts its `confetti` emitter (ConfettiTuning).
void RegisterStationReads(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
