#pragma once

#include <glm/glm.hpp>

#include "engine/platform/platform.hpp"

#include "render_components.hpp"

namespace engine::render {

/// A PointLight with the world-space position its entity's WorldTransform gives it.
struct PointLightSource {
	PointLight light;
	glm::vec3 position{0.0F};
};

/// Appends `source` to `params.point_lights`, in call order. Returns false, leaving `params` untouched, when
/// `LightParams::kMaxPointLights` are already packed (the light is dropped).
inline bool AddPointLight(platform::LightParams& params, const PointLightSource& source) {
	if (params.point_light_count >= platform::LightParams::kMaxPointLights) {
		return false;
	}
	const auto& [light, position] = source;
	params.point_lights[static_cast<std::size_t>(params.point_light_count++)] = {
		.position = position,
		.color = light.color,
		.intensity = light.intensity,
		.constant = light.constant,
		.linear = light.linear,
		.quadratic = light.quadratic,
	};
	return true;
}

} // namespace engine::render
