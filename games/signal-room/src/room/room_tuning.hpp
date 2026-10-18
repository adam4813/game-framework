#pragma once

#include <expected>
#include <string>

namespace signal_room {

// World singleton loaded from the tuning file's "movement" section: how Operators walk, turn, dash
// and collide. The Room panel edits it live (not saved to disk); a Reload re-reads the file.
struct MovementTuning {
	float walkSpeed{4.5F};  // m/s
	float accelTime{0.08F}; // s from standing to walk speed
	float decelTime{0.15F}; // s from walk speed to standing
	float turnTime{0.06F};  // s to turn half a circle toward the input
	float dashMultiplier{2.2F};
	float dashDuration{0.2F}; // s
	float dashCooldown{0.5F}; // s after a dash ends before the next can start
	float radius{0.3F};       // m, the Operator's collision circle
};

// World singleton loaded from the tuning file's "targeting" section: how an Operator picks its Target
// and how the Target brightens. Edited live by the Room panel like MovementTuning.
struct TargetingTuning {
	float rayLength{0.9F};        // m from the Operator's centre along its facing to the cell it faces
	float coneRange{1.2F};        // m, the fallback's reach to the nearest point of a workable thing
	float coneHalfAngle{50.0F};   // degrees either side of facing the fallback accepts
	float brightenRate{1.5F};     // pulses per second
	float brightenStrength{0.5F}; // peak blend (0-1) of the Target's colours toward the Operator's
	float brightenLift{0.35F};    // how far (0-1) the Operator's colour is lightened toward white first
};

// World singleton loaded from the tuning file's "dust_puff" section: the burst a set-down plays on the
// Counter. Edited live by the Room panel like MovementTuning.
struct DustPuffTuning {
	float count{14.0F};    // particles in the burst (rounded)
	float speed{1.2F};     // m/s
	float lifetime{0.45F}; // s
	float size{0.1F};      // m, each particle's starting radius
	float spread{0.9F};    // how far the burst fans out from straight up (0 is a column)
};

// Everything the tuning file holds, one singleton per section.
struct RoomTuning {
	MovementTuning movement;
	TargetingTuning targeting;
	DustPuffTuning dustPuff;
};

// Parse the tuning file; a missing section or field keeps its default. Every field must be a positive
// number, except the 0-1 fractions brighten_strength and brighten_lift. The error names the file and
// the bad field.
[[nodiscard]] std::expected<RoomTuning, std::string> LoadTuningFile(const std::string& path);

} // namespace signal_room
