#pragma once

#include <expected>
#include <string>

namespace signal_room {

// World singleton loaded from the tuning file's "movement" section: how Operators walk, turn, dash
// and collide. The Room panel edits it live (not saved to disk); a Reload re-reads the file.
struct MovementTuning {
	float walkSpeed{4.5F};     // m/s
	float accelTime{0.08F};    // s from standing to walk speed
	float decelTime{0.15F};    // s from walk speed to standing
	float turnTime{0.06F};     // s to turn half a circle toward the input
	float dashMultiplier{2.2F};
	float dashDuration{0.2F};  // s
	float dashCooldown{0.5F};  // s after a dash ends before the next can start
	float radius{0.3F};        // m, the Operator's collision circle
};

// Parse a tuning file's "movement" section; a missing field keeps its default. The error names the
// file and the field that isn't a positive number.
[[nodiscard]] std::expected<MovementTuning, std::string> LoadMovementTuningFile(const std::string& path);

} // namespace signal_room
