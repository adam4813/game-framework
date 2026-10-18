#include "movement_tuning.hpp"

#include <format>
#include <utility>

#include <nlohmann/json.hpp>

#include "json_file.hpp"

using nlohmann::json;

namespace signal_room {

std::expected<MovementTuning, std::string> LoadMovementTuningFile(const std::string& path) {
	const auto doc = ReadJsonFile(path, "Tuning");
	if (!doc) {
		return std::unexpected(doc.error());
	}

	const auto movement = doc->value("movement", json::object());
	MovementTuning tuning;
	for (const auto& [key, field] : {
			 std::pair{"walk_speed", &tuning.walkSpeed},
			 std::pair{"accel_time", &tuning.accelTime},
			 std::pair{"decel_time", &tuning.decelTime},
			 std::pair{"turn_time", &tuning.turnTime},
			 std::pair{"dash_multiplier", &tuning.dashMultiplier},
			 std::pair{"dash_duration", &tuning.dashDuration},
			 std::pair{"dash_cooldown", &tuning.dashCooldown},
			 std::pair{"radius", &tuning.radius},
		 }) {
		if (!movement.contains(key)) {
			continue;
		}
		const auto& value = movement.at(key);
		if (!value.is_number() || value.get<float>() <= 0.0F) {
			return std::unexpected(std::format("Tuning file {}: movement.{} must be a positive number", path, key));
		}
		*field = value.get<float>();
	}
	return tuning;
}

} // namespace signal_room
