#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <flecs.h>

#include "engine/spatial/spatial.hpp"

#include "render_components.hpp"
#include "camera_projection.hpp"

// TODO(scripting): expose ScreenProjector to AngelScript so scripts can project 3D world
// positions to screen-space pixels (e.g. to place UI labels above entities). Register as a
// value type with a Project(vec3) method and a CreateProjector(entity_handle) global.

namespace engine::render {

/// Projects 3D world-space positions to 2D screen-space pixels.
///
/// Captures the view and projection matrices at construction time; safe to copy, move, and
/// call from any system without touching the ECS world again.
///
/// Usage:
///   auto projector = render::ScreenProjector::FromCamera(cam_entity, w, h);
///   glm::vec2 screen = projector.Project(world_pos);
struct ScreenProjector {
	/// Project a 3D world-space position to 2D screen-space pixels.
	/// Returns (-9999, -9999) when the point is behind the camera (clip.w <= 0).
	[[nodiscard]] glm::vec2 Project(const glm::vec3& world_pos) const {
		const glm::vec4 clip = view_proj * glm::vec4(world_pos, 1.0f);
		if (clip.w <= 0.0f) return {-9999.0f, -9999.0f};
		const glm::vec3 ndc = glm::vec3(clip) / clip.w;
		return {(ndc.x + 1.0f) * 0.5f * viewport_w, (1.0f - ndc.y) * 0.5f * viewport_h};
	}

	[[nodiscard]] static ScreenProjector Build(
		const Camera& cam,
		const spatial::WorldTransform& cam_wt,
		const glm::vec3 look_target,
		const float viewport_w,
		const float viewport_h
	) {
		const glm::mat4 view = glm::lookAt(cam_wt.position, look_target, cam.up);
		const glm::mat4 proj = BuildProjection(cam, cam.aspect_ratio);
		return ScreenProjector{.view_proj = proj * view, .viewport_w = viewport_w, .viewport_h = viewport_h};
	}

	/// Build a projector directly from a camera entity, resolving the LookAt target automatically.
	/// Falls back to the camera's own forward axis when no LookAt target is present.
	[[nodiscard]] static ScreenProjector
	FromCamera(const flecs::entity cam_e, const float viewport_w, const float viewport_h) {
		const auto& wt = cam_e.get<const spatial::WorldTransform>();
		const auto& cam = cam_e.get<const Camera>();
		glm::vec3 look_target = wt.position + glm::vec3(wt.matrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
		if (const auto look = cam_e.target<LookAt>(); look && look.has<spatial::WorldTransform>()) {
			look_target = look.get<const spatial::WorldTransform>().position;
		}
		return Build(cam, wt, look_target, viewport_w, viewport_h);
	}

	glm::mat4 view_proj{1.0f};
	float viewport_w{1.0f};
	float viewport_h{1.0f};
};

} // namespace engine::render
