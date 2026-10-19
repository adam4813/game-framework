#pragma once

#include <expected>
#include <string>

#include <flecs.h>

#include "engine/core/core.hpp"
#include "shift.hpp"

namespace signal_room {

// Give every in-service Radio under `parent` its Shift frequency, and reset the Antenna Coupler. Call
// before the greybox is built: the Radio's body and display take the frequency's colour. The error names
// `shiftPath` and the Radio that has no frequency, or the Shift entry that names no Station.
[[nodiscard]] std::expected<void, std::string>
AssignRadios(const flecs::world& world, flecs::entity parent, const Shift& shift, const std::string& shiftPath);

// The colour of a frequency label from the frequency table and palette, or nullptr when either lacks it.
const engine::core::Rgba* FrequencyColour(const flecs::world& world, const std::string& label);

// The duller colour of the Radio set itself (palette token `<frequency colour>_body`), or the bright one if absent.
const engine::core::Rgba* FrequencyBodyColour(const flecs::world& world, const std::string& label);

// Couple the antenna to `radio`: every other Radio goes dark until the coupler disconnects. Raises
// CouplerEngaged on the Radio. Precondition: the Radio is online.
void EngageCoupler(const flecs::world& world, flecs::entity radio);

// Register the Radio systems under the Room's root: the Coupler reset clock, each Radio's derived online
// state, the disconnect on a Hold ending, the dimming of dark Radios and their frequency labels.
void RegisterRadioSystems(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
