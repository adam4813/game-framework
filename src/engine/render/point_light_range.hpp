#pragma once

#include <algorithm>
#include <cmath>

#include "engine/platform/platform.hpp"

#include "render_components.hpp"

namespace engine::render {

// A light derives its range as the distance where attenuation times intensity falls to this, one step of an 8-bit
// color channel.
constexpr float kDerivedRangeCutoff = 1.0F / 256.0F;

// The longest range a light derives, for one whose terms fade slowly or never: as far as anything is drawn.
constexpr float kMaxDerivedRange = platform::kClipFar;

// The distance at which `light` ends: its `range` when set, otherwise the one its attenuation terms give. 0 for a light
// already below the cutoff at its own position.
[[nodiscard]] inline float EffectiveRange(const PointLight& light) {
	if (light.range > 0.0F) return light.range;
	// The cutoff is where constant + linear*d + quadratic*d^2 reaches `target`.
	const float target = std::abs(light.intensity) / kDerivedRangeCutoff;
	const float rise = target - light.constant;
	if (rise <= 0.0F) return 0.0F;
	if (light.quadratic > 0.0F) {
		const float root = std::sqrt(light.linear * light.linear + 4.0F * light.quadratic * rise);
		return std::min((root - light.linear) / (2.0F * light.quadratic), kMaxDerivedRange);
	}
	if (light.linear > 0.0F) return std::min(rise / light.linear, kMaxDerivedRange);
	return kMaxDerivedRange;
}

} // namespace engine::render
