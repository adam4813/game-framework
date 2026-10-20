#include "room_tuning.hpp"

#include <format>
#include <initializer_list>
#include <utility>

#include <nlohmann/json.hpp>

#include "json_file.hpp"

using nlohmann::json;

namespace signal_room {

namespace {

// What a tuning field accepts: a positive number, or a fraction from 0 to 1.
enum class Range { Positive, Fraction };

struct Field {
	const char* key;
	float* value;
	Range range{Range::Positive};
};

// Read each present field of `doc[section]` into its float, checking it against its range.
std::expected<void, std::string>
ReadFields(const json& doc, const char* section, const std::initializer_list<Field> fields, const std::string& path) {
	const auto values = doc.value(section, json::object());
	for (const auto& [key, field, range] : fields) {
		if (!values.contains(key)) {
			continue;
		}
		const auto& value = values.at(key);
		// A non-number reads as -1, outside both ranges.
		const float number = value.is_number() ? value.get<float>() : -1.0F;
		if (range == Range::Positive && number <= 0.0F) {
			return std::unexpected(std::format("Tuning file {}: {}.{} must be a positive number", path, section, key));
		}
		if (range == Range::Fraction && (number < 0.0F || number > 1.0F)) {
			return std::unexpected(
				std::format("Tuning file {}: {}.{} must be a number from 0 to 1", path, section, key)
			);
		}
		*field = number;
	}
	return {};
}

} // namespace

std::expected<RoomTuning, std::string> LoadTuningFile(const std::string& path, const ActiveModifiers& modifiers) {
	const auto doc = ReadGameDocument(path, "Tuning", "tuning", modifiers);
	if (!doc) {
		return std::unexpected(doc.error());
	}

	RoomTuning tuning;
	auto& movement = tuning.movement;
	if (auto read = ReadFields(
			*doc,
			"movement",
			{
				{"walk_speed", &movement.walkSpeed},
				{"accel_time", &movement.accelTime},
				{"decel_time", &movement.decelTime},
				{"turn_time", &movement.turnTime},
				{"dash_multiplier", &movement.dashMultiplier},
				{"dash_duration", &movement.dashDuration},
				{"dash_cooldown", &movement.dashCooldown},
				{"radius", &movement.radius},
			},
			path
		);
		!read) {
		return std::unexpected(read.error());
	}

	auto& targeting = tuning.targeting;
	if (auto read = ReadFields(
			*doc,
			"targeting",
			{
				{"ray_length", &targeting.rayLength},
				{"cone_range", &targeting.coneRange},
				{"cone_half_angle", &targeting.coneHalfAngle},
				{"brighten_rate", &targeting.brightenRate},
				{"brighten_strength", &targeting.brightenStrength, Range::Fraction},
				{"brighten_lift", &targeting.brightenLift, Range::Fraction},
			},
			path
		);
		!read) {
		return std::unexpected(read.error());
	}

	auto& dustPuff = tuning.dustPuff;
	if (auto read = ReadFields(
			*doc,
			"dust_puff",
			{
				{"count", &dustPuff.count},
				{"speed", &dustPuff.speed},
				{"lifetime", &dustPuff.lifetime},
				{"size", &dustPuff.size},
				{"spread", &dustPuff.spread},
			},
			path
		);
		!read) {
		return std::unexpected(read.error());
	}
	auto& confetti = tuning.confetti;
	if (auto read = ReadFields(
			*doc,
			"confetti",
			{
				{"count", &confetti.count},
				{"speed", &confetti.speed},
				{"lifetime", &confetti.lifetime},
				{"size", &confetti.size},
				{"spread", &confetti.spread},
			},
			path
		);
		!read) {
		return std::unexpected(read.error());
	}
	auto& reads = tuning.reads;
	if (auto read = ReadFields(
			*doc,
			"reads",
			{
				{"blink_slow", &reads.blinkSlow},
				{"blink_quick", &reads.blinkQuick},
				{"blink_min", &reads.blinkMin},
				{"blink_max", &reads.blinkMax},
				{"reel_speed", &reads.reelSpeed},
				{"reel_cranked_speed", &reads.reelCrankedSpeed},
				{"crank_speed", &reads.crankSpeed},
				{"telex_flash", &reads.telexFlash},
				{"telex_slide", &reads.telexSlide},
				{"dark_dim", &reads.darkDim, Range::Fraction},
			},
			path
		);
		!read) {
		return std::unexpected(read.error());
	}
	auto& guidance = tuning.guidance;
	if (auto read = ReadFields(
			*doc,
			"guidance",
			{
				{"chevron_height", &guidance.chevronHeight},
				{"chevron_bob", &guidance.chevronBob},
				{"chevron_bob_rate", &guidance.chevronBobRate},
				{"chevron_spacing", &guidance.chevronSpacing},
				{"bubble_duration", &guidance.bubbleDuration},
				{"bubble_side", &guidance.bubbleSide},
				{"bubble_height", &guidance.bubbleHeight},
			},
			path
		);
		!read) {
		return std::unexpected(read.error());
	}
	auto& coupler = tuning.coupler;
	if (auto read = ReadFields(*doc, "coupler", {{"coupler_reset", &coupler.couplerReset}}, path); !read) {
		return std::unexpected(read.error());
	}
	return tuning;
}

} // namespace signal_room
