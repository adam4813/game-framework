#pragma once

#include <string>

#include <flecs.h>

namespace signal_room {

// On each in-service Radio: the frequency its Shift tuned it to (a label from the frequency table) and
// whether it is online right now. `online` is derived every frame (RadioOnlineState): an On Air Ticket
// names the frequency, the Antenna Coupler is free or coupled to this Radio, and no Coupler reset runs.
struct Radio {
	std::string frequency;
	bool online{false};
};

// World singleton: the room's one antenna. `coupled` is the Radio an Operator is copying at (0 = free);
// `resetLeft` is the seconds of dark left after the last disconnect, during which every Radio is offline.
struct AntennaCoupler {
	flecs::entity_t coupled{0};
	float resetLeft{0.0F};
};

// Events raised on the Radio, for the audio spec to observe.
struct CouplerEngaged {};
struct CouplerReleased {};
struct RadioOnline {};

} // namespace signal_room
