#include "render_queue.hpp"

#include <glm/geometric.hpp>

namespace engine::render {

void Frustum::FromViewProj(const glm::mat4& vp) {
	// glm is column-major, so row i of the matrix is (vp[0][i], vp[1][i], vp[2][i], vp[3][i]).
	const glm::vec4 row0{vp[0][0], vp[1][0], vp[2][0], vp[3][0]};
	const glm::vec4 row1{vp[0][1], vp[1][1], vp[2][1], vp[3][1]};
	const glm::vec4 row2{vp[0][2], vp[1][2], vp[2][2], vp[3][2]};
	const glm::vec4 row3{vp[0][3], vp[1][3], vp[2][3], vp[3][3]};

	planes[0] = row3 + row0; // left
	planes[1] = row3 - row0; // right
	planes[2] = row3 + row1; // bottom
	planes[3] = row3 - row1; // top
	planes[4] = row3 + row2; // near
	planes[5] = row3 - row2; // far

	for (glm::vec4& p : planes) {
		const float len = glm::length(glm::vec3(p));
		if (len > 0.0f) {
			p /= len;
		}
	}
}

bool Frustum::ContainsSphere(const glm::vec3 center, const float radius) const {
	for (const glm::vec4& p : planes) {
		const float distance = glm::dot(glm::vec3(p), center) + p.w;
		if (distance < -radius) {
			return false; // fully outside this plane
		}
	}
	return true;
}

} // namespace engine::render
