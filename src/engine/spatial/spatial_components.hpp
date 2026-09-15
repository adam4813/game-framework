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
//
// TODO(perf/design): consider replacing `rotation` (Euler vec3) with an `orientation` quaternion.
// Physics (Jolt) and matrix extraction (glm::quat_cast) both produce quaternions directly, so an
// orientation quat would remove the eulerAngles() conversions in the parented-extraction and
// physics-sync paths and avoid gimbal ambiguity. Note the tradeoff: the TransformPropagation root
// fast-path currently copies the authored Euler `rotation` for free; a quat would add a per-entity
// euler->quat conversion there, so weigh the parented/physics win against the root-path cost. Touch
// points: this struct + ComputeMatrix, spatial_module reflection/seed/propagation, jolt_module sync.
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
