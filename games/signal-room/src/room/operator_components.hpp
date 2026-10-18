#pragma once

#include <string>

#include <glm/glm.hpp>

namespace signal_room {

// An Operator on the bunker floor, spawned from a Shift's Roster. Its floor position is its
// Transform's x and z (y stays 0). `colour` is its identity colour's palette token. `carrySpeed`
// scales its walk and dash: 1.0 until Items exist (ticket 05 sets it from the carried Item).
struct Operator {
	std::string colour;
	float carrySpeed{1.0F};
};

// What an Operator is asked to do this frame: `move` is the wish direction on the floor plane (x east,
// y south; length at most 1), `dash` and `interact` are presses. The OperatorInput system fills it
// from the keyboard Bindings; spec 3's Seats and AI write it instead. Over REST, disable OperatorInput
// and set it to drive an Operator by hand.
struct OperatorActions {
	glm::vec2 move{0.0F};
	bool dash{false};
	bool interact{false};
};

// How an Operator is moving: `velocity` on the floor plane (x east, y south, in m/s) and `facing` as
// a yaw in radians about +y (0 faces north, -z; the Transform's rotation.y follows it).
struct OperatorMotion {
	glm::vec2 velocity{0.0F};
	float facing{0.0F};
};

// An Operator's dash: `timeLeft` > 0 while dashing along `direction` (a unit floor vector), and
// `cooldownLeft` > 0 while the next dash must wait. The cooldown starts when the dash ends.
struct Dash {
	glm::vec2 direction{0.0F};
	float timeLeft{0.0F};
	float cooldownLeft{0.0F};
};

} // namespace signal_room
