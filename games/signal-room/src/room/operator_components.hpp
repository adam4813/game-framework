#pragma once

#include <cmath>
#include <string>

#include <glm/glm.hpp>

namespace signal_room {

// An Operator on the bunker floor, spawned from a Shift's Roster. Its floor position is its
// Transform's x and z (y stays 0). `colour` is its identity colour's palette token. What it carries
// is its (Carries, item) pair (item_components.hpp).
struct Operator {
	std::string colour;
};

// What an Operator is asked to do this frame: `move` is the wish direction on the floor plane (x east,
// y south; length at most 1), `dash` and `interact` are presses and `interactHeld` stays true while the key stays down. The OperatorInput system fills it
// from the keyboard Bindings; spec 3's Seats and AI write it instead. Over REST, disable OperatorInput
// and set it to drive an Operator by hand.
struct OperatorActions {
	glm::vec2 move{0.0F};
	bool dash{false};
	bool interact{false};
	bool interactHeld{false}; // the action key is down, from the press on
};

// How an Operator is moving: `velocity` on the floor plane (x east, y south, in m/s) and `facing` as
// a yaw in radians about +y (0 faces north, -z; the Transform's rotation.y follows it).
struct OperatorMotion {
	glm::vec2 velocity{0.0F};
	float facing{0.0F};
};

// The unit floor vector (x east, y south) an OperatorMotion facing yaw points along.
inline glm::vec2 FacingVector(const float yaw) { return {-std::sin(yaw), -std::cos(yaw)}; }

// An Operator's dash: `timeLeft` > 0 while dashing along `direction` (a unit floor vector), and
// `cooldownLeft` > 0 while the next dash must wait. The cooldown starts when the dash ends.
struct Dash {
	glm::vec2 direction{0.0F};
	float timeLeft{0.0F};
	float cooldownLeft{0.0F};
};

// Relationship from an Operator to its Target: the Station or Counter it would act on if it pressed
// now, as (Targets, entity). Exclusive, so an Operator has at most one; no pair means no Target. The
// OperatorTargeting system sets it every frame; verbs, Hints and chevrons read it.
struct Targets {};

} // namespace signal_room
