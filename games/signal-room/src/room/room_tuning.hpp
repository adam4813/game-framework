#pragma once

#include <expected>
#include <string>

#include "modifiers.hpp"

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

// World singleton loaded from the tuning file's "confetti" section: the burst the Telex plays on each
// transmission, fields as DustPuffTuning.
struct ConfettiTuning {
	float count{40.0F};
	float speed{2.2F};
	float lifetime{0.9F};
	float size{0.06F};
	float spread{1.2F};
};

// World singleton loaded from the tuning file's "reads" section: how Stations show their state.
struct ReadTuning {
	float blinkSlow{1.0F};         // blinks per second, a Radio On Air
	float blinkQuick{6.0F};        // blinks per second, a Radio in a Coupler reset
	float blinkMin{1.0F};          // blinks per second of a Running lamp at progress 0
	float blinkMax{6.0F};          // blinks per second of a Running lamp at progress 1
	float reelSpeed{4.0F};         // rad/s, the Decoder's reels while Running
	float reelCrankedSpeed{14.0F}; // rad/s, while cranked
	float crankSpeed{9.0F};        // rad/s, the crank while cranked
	float telexFlash{0.5F};        // s, the Telex's green flash and paper slide after a transmission
	float telexSlide{0.4F};        // m, how far the paper slides up
	float darkDim{0.45F};          // fraction (0-1) of its colour a dark Radio's body keeps
};

// World singleton loaded from the tuning file's "coupler" section: the Antenna Coupler. Edited live by the Room
// panel like MovementTuning.
struct CouplerTuning {
	float couplerReset{0.4F}; // s every Radio stays dark after the coupler disconnects (keep under 0.5)
};

// Everything the tuning file holds, one singleton per section.
struct RoomTuning {
	MovementTuning movement;
	TargetingTuning targeting;
	DustPuffTuning dustPuff;
	CouplerTuning coupler;
	ConfettiTuning confetti;
	ReadTuning reads;
};

// Parse the tuning file; a missing section or field keeps its default. Every field must be a positive
// number, except the 0-1 fractions brighten_strength and brighten_lift. The error names the file and
// the bad field.
[[nodiscard]] std::expected<RoomTuning, std::string> LoadTuningFile(const std::string& path, const ActiveModifiers& modifiers = {});

} // namespace signal_room
