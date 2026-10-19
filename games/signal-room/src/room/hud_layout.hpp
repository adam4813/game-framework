#pragma once

#include <expected>
#include <string>
#include <unordered_map>

#include "engine/core/core.hpp"
#include "palette.hpp"

namespace signal_room {

// World singleton: the in-play HUD's layout numbers, strings and resolved colours, loaded from the HUD file.
struct HudLayout {
	float margin{16.0F};
	float cardWidth{190.0F};
	float cardHeight{92.0F};
	float cardSpacing{8.0F};
	float cardPadding{8.0F};
	float gaugeHeight{14.0F};
	float swatchSize{14.0F};

	float callsignFont{22.0F};
	float tierFont{13.0F};
	float frequencyFont{18.0F};
	float clockFont{40.0F};
	float tagFont{18.0F};
	float scoreFont{32.0F};
	float chainFont{26.0F};

	float exitFlash{0.6F};   // s a Cleared or Lost card flashes before it goes
	float redFlashHz{3.0F};  // flashes per second of a red-band gauge
	float popSeconds{0.3F};  // s a score or chain label stays scaled
	float popScale{1.35F};

	std::string lastCallLabel{"LAST CALL"};
	std::string chainPrefix{"x"};

	engine::core::Rgba panel{};
	engine::core::Rgba border{};
	engine::core::Rgba text{};
	engine::core::Rgba textDim{};
	engine::core::Rgba track{};
	engine::core::Rgba standby{};
	engine::core::Rgba flashClear{};
	engine::core::Rgba flashLost{};
	engine::core::Rgba lastCall{};
	std::unordered_map<std::string, engine::core::Rgba> bands; // gauge band id -> colour
};

// Parse the HUD file, resolving its colour tokens against `palette`. The error names the file and the bad entry.
[[nodiscard]] std::expected<HudLayout, std::string> LoadHudLayoutFile(const std::string& path, const Palette& palette);

} // namespace signal_room
