#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "engine/render/render_components.hpp"

namespace engine::render {

// Builds the projection matrix for a camera given the viewport aspect ratio (width / height).
//
// Perspective cameras use glm::perspective with the camera's vertical `fov`. Orthographic cameras
// use glm::ortho with a symmetric frustum whose vertical extent is `ortho_size` world units and
// whose horizontal extent scales with `aspect` (halfWidth = halfHeight * aspect). `near_plane` and
// `far_plane` carry through in both modes.
//
// Pure and GPU-free: the render pipeline uses it for the cull frustum and ScreenProjector, and the
// unit tests assert the ortho matrix without a world or backend.
[[nodiscard]] inline glm::mat4 BuildProjection(const Camera& camera, const float aspect) {
	if (camera.projection == Projection::Orthographic) {
		const float halfHeight = camera.ortho_size * 0.5F;
		const float halfWidth = halfHeight * aspect;
		return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, camera.near_plane, camera.far_plane);
	}
	return glm::perspective(glm::radians(camera.fov), aspect, camera.near_plane, camera.far_plane);
}

} // namespace engine::render
