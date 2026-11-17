#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/glm.hpp>

#include "engine/platform/platform.hpp"

// The translucent pass's draw list. The render module collects one TranslucentDraw per visible
// Translucent entity each frame, sorts the list back to front, and issues the draws after all
// opaque geometry with depth write off (see Translucent in render_components.hpp).
namespace engine::render {

// Which platform draw call a TranslucentDraw issues. Closed: one per renderable shape the engine has.
enum class TranslucentShape : uint8_t { Cube, Sphere, Quad, Capsule, Mesh, SkinnedMesh, DynamicMesh };

// One deferred translucent draw: everything the platform call needs, captured at collection time.
struct TranslucentDraw {
	TranslucentShape shape{TranslucentShape::Cube};
	glm::vec3 center{0.0F};      // world-space point the sort measures from (the entity's position)
	glm::mat4 transform{1.0F};   // the entity's WorldTransform matrix
	glm::vec3 size{1.0F};        // Cube: size; Sphere: x = radius; Quad: x,y = size; Capsule: x = radius, y = height
	int handle{-1};              // Mesh / SkinnedMesh: model handle; DynamicMesh: dynamic mesh handle
	int skeletal_set{-1};        // SkinnedMesh only
	platform::SkeletalPose pose; // SkinnedMesh only
	platform::DrawMaterial material;
};

// Orders `draws` back to front: farthest from `eye` first, so each blended draw lands over everything
// behind it. Draws at equal distance keep their collection order (a stable sort), so the result
// doesn't flicker between frames.
inline void SortBackToFront(std::vector<TranslucentDraw>& draws, const glm::vec3 eye) {
	std::ranges::stable_sort(draws, [eye](const TranslucentDraw& a, const TranslucentDraw& b) {
		const glm::vec3 da = a.center - eye;
		const glm::vec3 db = b.center - eye;
		return glm::dot(da, da) > glm::dot(db, db);
	});
}

} // namespace engine::render
