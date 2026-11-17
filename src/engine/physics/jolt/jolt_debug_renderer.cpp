#include "jolt_debug_renderer.hpp"

#include <glm/glm.hpp>

namespace engine::physics {

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER

namespace {

// Raycast colors: the ray up to its nearest hit (or its whole length on a miss), the rest of a hit ray, the hit-point
// marker and the surface normal.
constexpr core::Rgba kRayToHitColor{.r = 80, .g = 255, .b = 80, .a = 255};
constexpr core::Rgba kRayMissColor{.r = 255, .g = 220, .b = 60, .a = 255};
constexpr core::Rgba kRayPastHitColor{.r = 120, .g = 120, .b = 120, .a = 255};
constexpr core::Rgba kHitPointColor{.r = 255, .g = 60, .b = 60, .a = 255};
constexpr core::Rgba kNormalColor{.r = 60, .g = 200, .b = 255, .a = 255};
// World-space sizes of the hit-point cross (half its arm) and the normal line.
constexpr float kHitMarkerHalfSize = 0.1F;
constexpr float kNormalLength = 0.5F;

glm::vec3 ToGlm(JPH::RVec3Arg v) {
	return {static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ())};
}

core::Rgba ToRgba(const JPH::ColorArg c) { return {.r = c.r, .g = c.g, .b = c.b, .a = c.a}; }

} // namespace

JoltDebugRendererAdapter::JoltDebugRendererAdapter() { Initialize(); }

void JoltDebugRendererAdapter::DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, const JPH::ColorArg color) {
	const std::scoped_lock lock(mutex_);
	if (!enabled_) return;
	body_lines_.push_back({.from = ToGlm(from), .to = ToGlm(to), .color = ToRgba(color)});
}

void JoltDebugRendererAdapter::DrawTriangle(
	JPH::RVec3Arg v1,
	JPH::RVec3Arg v2,
	JPH::RVec3Arg v3,
	const JPH::ColorArg color,
	ECastShadow /*cast_shadow*/
) {
	DrawLine(v1, v2, color);
	DrawLine(v2, v3, color);
	DrawLine(v3, v1, color);
}

void JoltDebugRendererAdapter::DrawText3D(
	JPH::RVec3Arg /*position*/,
	const std::string_view& /*text*/,
	const JPH::ColorArg /*color*/,
	float /*height*/
) {}

void JoltDebugRendererAdapter::SetEnabled(const bool enabled) {
	const std::scoped_lock lock(mutex_);
	enabled_ = enabled;
	if (!enabled) {
		body_lines_.clear();
		pending_rays_.clear();
		shown_rays_.clear();
	}
}

bool JoltDebugRendererAdapter::Enabled() const {
	const std::scoped_lock lock(mutex_);
	return enabled_;
}

void JoltDebugRendererAdapter::RecordRay(
	const glm::vec3 origin,
	const glm::vec3 end,
	const std::span<const RaycastResult> hits
) {
	const std::scoped_lock lock(mutex_);
	if (!enabled_) return;
	if (hits.empty()) {
		pending_rays_.push_back({.from = origin, .to = end, .color = kRayMissColor});
		return;
	}
	pending_rays_.push_back({.from = origin, .to = hits.front().hit_point, .color = kRayToHitColor});
	pending_rays_.push_back({.from = hits.front().hit_point, .to = end, .color = kRayPastHitColor});
	for (const auto& hit : hits) {
		const glm::vec3 p = hit.hit_point;
		for (const glm::vec3 axis : {glm::vec3{1, 0, 0}, glm::vec3{0, 1, 0}, glm::vec3{0, 0, 1}}) {
			const glm::vec3 arm = axis * kHitMarkerHalfSize;
			pending_rays_.push_back({.from = p - arm, .to = p + arm, .color = kHitPointColor});
		}
		pending_rays_.push_back({.from = p, .to = p + hit.normal * kNormalLength, .color = kNormalColor});
	}
}

void JoltDebugRendererAdapter::Flush(std::vector<render::Line3D>& out) {
	const std::scoped_lock lock(mutex_);
	if (!pending_rays_.empty()) {
		shown_rays_.swap(pending_rays_);
		pending_rays_.clear();
	}
	out.insert(out.end(), body_lines_.begin(), body_lines_.end());
	out.insert(out.end(), shown_rays_.begin(), shown_rays_.end());
	body_lines_.clear();
}

#endif // ENGINE_PHYSICS_DEBUG_RENDERER

} // namespace engine::physics
