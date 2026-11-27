#include "occluder_fade.hpp"

#include <algorithm>
#include <limits>

#include <glm/geometric.hpp>

namespace game::farm {

namespace {

// Ground-plane depth gap below which a quad counts as level with the focus, not in front of it.
constexpr float kInFrontEpsilon = 1e-3F;
// Screen overlap must exceed this, so a neighboring column's quad that only touches the focus box's
// edge doesn't fade.
constexpr float kOverlapEpsilon = 1e-4F;

// A screen-space rectangle on the camera's right/up axes.
struct ScreenRect {
	glm::vec2 min{std::numeric_limits<float>::max()};
	glm::vec2 max{std::numeric_limits<float>::lowest()};

	void Add(const glm::vec2 p) {
		min = glm::min(min, p);
		max = glm::max(max, p);
	}

	[[nodiscard]] bool Overlaps(const ScreenRect& o) const {
		return min.x + kOverlapEpsilon < o.max.x && o.min.x + kOverlapEpsilon < max.x
			&& min.y + kOverlapEpsilon < o.max.y && o.min.y + kOverlapEpsilon < max.y;
	}
};

// The orthographic screen axes for a camera looking along `forward` with world up +Y.
struct ScreenAxes {
	glm::vec3 right;
	glm::vec3 up;

	[[nodiscard]] glm::vec2 Project(const glm::vec3 p) const { return {glm::dot(p, right), glm::dot(p, up)}; }
};

ScreenAxes MakeScreenAxes(const glm::vec3 forward) {
	const glm::vec3 f = glm::normalize(forward);
	const glm::vec3 right = glm::normalize(glm::cross(f, glm::vec3{0.0F, 1.0F, 0.0F}));
	return {.right = right, .up = glm::cross(right, f)};
}

} // namespace

std::vector<size_t> FindOccluders(const std::vector<WorldObject>& objects, const OccluderView& view) {
	std::vector<size_t> result;

	// A camera looking straight down has no ground-plane forward and nothing stands in front of anything.
	const glm::vec2 groundForward{view.viewDir.x, view.viewDir.z};
	if (glm::dot(groundForward, groundForward) < 1e-8F) {
		return result;
	}
	const glm::vec2 forward = glm::normalize(groundForward);
	const ScreenAxes axes = MakeScreenAxes(view.viewDir);

	ScreenRect focusRect;
	for (int corner = 0; corner < 8; ++corner) {
		const glm::vec3 sign{(corner & 1) ? 1.0F : -1.0F, (corner & 2) ? 1.0F : -1.0F, (corner & 4) ? 1.0F : -1.0F};
		focusRect.Add(axes.Project(view.focusCenter + sign * view.focusHalfExtents));
	}
	const float focusDepth = glm::dot(glm::vec2{view.focusCenter.x, view.focusCenter.z}, forward);
	const float halfWidth = view.quadWidthTiles * view.tileSize * 0.5F;

	for (size_t i = 0; i < objects.size(); ++i) {
		const WorldObject& obj = objects[i];
		if (obj.height <= kOccluderMinHeightTiles) {
			continue;
		}
		const float height = obj.height * view.tileSize;
		const bool occludes = std::ranges::any_of(obj.footprint, [&](const glm::ivec2 tile) {
			const glm::vec3 base{
				(static_cast<float>(tile.x) + 0.5F) * view.tileSize, 0.0F, (static_cast<float>(tile.y) + 0.5F) * view.tileSize
			};
			if (glm::dot(glm::vec2{base.x, base.z}, forward) > focusDepth - kInFrontEpsilon) {
				return false; // level with or behind the focus
			}
			ScreenRect quad;
			quad.Add(axes.Project(base + glm::vec3{-halfWidth, 0.0F, 0.0F}));
			quad.Add(axes.Project(base + glm::vec3{halfWidth, 0.0F, 0.0F}));
			quad.Add(axes.Project(base + glm::vec3{-halfWidth, height, 0.0F}));
			quad.Add(axes.Project(base + glm::vec3{halfWidth, height, 0.0F}));
			return quad.Overlaps(focusRect);
		});
		if (occludes) {
			result.push_back(i);
		}
	}
	return result;
}

} // namespace game::farm
