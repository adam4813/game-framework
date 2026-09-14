#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace engine::spatial {

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

} // namespace engine::spatial
