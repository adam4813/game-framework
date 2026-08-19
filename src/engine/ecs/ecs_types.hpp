#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace engine::ecs {

// Transform component: position, rotation (Euler radians), and scale, local to the parent.
struct Transform {
	glm::vec3 position{0.0F};
	glm::vec3 rotation{0.0F};
	glm::vec3 scale{1.0F};
};

// WorldTransform component: world-space position, rotation, and scale.
struct WorldTransform {
	glm::vec3 position{0.0F};
	glm::vec3 rotation{0.0F};
	glm::vec3 scale{1.0F};
	glm::mat4 matrix{1.0F};

	void ComputeMatrix() {
		const glm::mat4 t = glm::translate(glm::mat4(1.0F), position);
		const glm::mat4 rx = glm::rotate(glm::mat4(1.0F), rotation.x, glm::vec3(1, 0, 0));
		const glm::mat4 ry = glm::rotate(glm::mat4(1.0F), rotation.y, glm::vec3(0, 1, 0));
		const glm::mat4 rz = glm::rotate(glm::mat4(1.0F), rotation.z, glm::vec3(0, 0, 1));
		const glm::mat4 s = glm::scale(glm::mat4(1.0F), scale);
		matrix = t * rz * ry * rx * s;
	}
};

// Factory: build a WorldTransform with its matrix precomputed from position/rotation/scale.
[[nodiscard]] inline WorldTransform
MakeWorldTransform(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale) {
	WorldTransform wt{.position = position, .rotation = rotation, .scale = scale};
	wt.ComputeMatrix();
	return wt;
}

// Engine-defined pipeline phase: runs after all flecs::OnStore systems complete.
// Use this phase (via `.kind<ecs::OnUI>()`) for 2D overlays, HUD, buttons, and
// debug panels that must composite on top of 3D geometry. UI *rendering* (building and flushing
// the 2D draw list) runs here; UI *layout* runs in PreUpdate and UI *interaction* in OnUpdate,
// like any other simulation, so input is consumed before InputResetFrameState clears it.
struct OnUI {};

} // namespace engine::ecs
