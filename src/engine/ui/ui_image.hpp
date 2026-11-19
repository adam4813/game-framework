#pragma once

#include <algorithm>

#include <glm/glm.hpp>

#include "engine/core/core.hpp"
#include "ui_components.hpp"

namespace engine::ui {

// The texel region and screen rectangle a textured draw resolves to: `src` in texels, `dest` in pixels.
struct ImageRects {
	core::Rect src{};
	core::Rect dest{};
};

// Resolves how `fit` maps `source` (a texel sub-rect; a zero width or height means the whole texture) into
// `dest`, given the texture's size in texels. Pure, so the fit math is tested without a window. When the
// texture's size is unknown (zero) and no source rect is given, the result is a Tile, as nothing better
// can be computed.
[[nodiscard]] inline ImageRects
ResolveImageRects(const FitMode fit, const core::Rect dest, const core::Rect source, const glm::vec2 texture_size) {
	const bool has_source = source.w > 0.0F && source.h > 0.0F;
	if (!has_source && (texture_size.x <= 0.0F || texture_size.y <= 0.0F)) {
		return {.src = {.x = 0.0F, .y = 0.0F, .w = dest.w, .h = dest.h}, .dest = dest};
	}
	const core::Rect src = has_source ? source : core::Rect{.x = 0.0F, .y = 0.0F, .w = texture_size.x, .h = texture_size.y};

	switch (fit) {
	case FitMode::Stretch: return {.src = src, .dest = dest};
	case FitMode::Tile: return {.src = {.x = src.x, .y = src.y, .w = dest.w, .h = dest.h}, .dest = dest};
	case FitMode::Fit:
	{
		const float scale = std::min(dest.w / src.w, dest.h / src.h);
		const float w = src.w * scale;
		const float h = src.h * scale;
		return {.src = src, .dest = {.x = dest.x + (dest.w - w) / 2.0F, .y = dest.y + (dest.h - h) / 2.0F, .w = w, .h = h}};
	}
	case FitMode::Fill:
	{
		const float scale = std::max(dest.w / src.w, dest.h / src.h);
		const float w = dest.w / scale;
		const float h = dest.h / scale;
		return {.src = {.x = src.x + (src.w - w) / 2.0F, .y = src.y + (src.h - h) / 2.0F, .w = w, .h = h}, .dest = dest};
	}
	}
	return {.src = src, .dest = dest};
}

} // namespace engine::ui
