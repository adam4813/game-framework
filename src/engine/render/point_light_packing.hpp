#pragma once

#include <algorithm>
#include <span>

#include <glm/glm.hpp>

#include "engine/platform/platform.hpp"

#include "render_components.hpp"

namespace engine::render {

/// A PointLight with the world-space position its entity's WorldTransform gives it.
struct PointLightSource {
	PointLight light;
	glm::vec3 position{0.0F};
};

/// Packs `sources` into `params.point_lights` in order and sets `params.point_light_count`.
/// Stops at `LightParams::kMaxPointLights`; returns how many sources it dropped.
inline int PackPointLights(const std::span<const PointLightSource> sources, platform::LightParams& params) {
	const int packed = static_cast<int>(std::min<std::size_t>(sources.size(), platform::LightParams::kMaxPointLights));
	for (int i = 0; i < packed; ++i) {
		const auto& [light, position] = sources[static_cast<std::size_t>(i)];
		params.point_lights[static_cast<std::size_t>(i)] = {
			.position = position,
			.color = light.color,
			.intensity = light.intensity,
			.constant = light.constant,
			.linear = light.linear,
			.quadratic = light.quadratic,
		};
	}
	params.point_light_count = packed;
	return static_cast<int>(sources.size()) - packed;
}

} // namespace engine::render
