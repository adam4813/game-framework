#pragma once

#include <glm/glm.hpp>

namespace engine::core {

struct Rect {
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;

	[[nodiscard]] bool Contains(const glm::vec2 p) const {
		return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
	}
};

struct Rgba {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 255;
};

} // namespace engine::core
