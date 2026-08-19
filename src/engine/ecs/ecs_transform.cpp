#include <flecs.h>
#include <glm/glm.hpp>

#include "ecs_types.hpp"
#include "engine/physics/physics.hpp"
#include "engine/scene/scene.hpp"

namespace engine::ecs {

// Register the transform propagation system with Flecs.
// This system runs in PreStore to cascade Transform changes to WorldTransform for entities
// that have both components (the typical case for entities modified via input or scripts).
void RegisterTransformPropagation(const flecs::world& world) {
	// === SYSTEM: Propagate Transform to WorldTransform ===
	// Runs in PreStore (before rendering) to update WorldTransform from Transform for entities
	// that have both. This allows input handlers and scripts to modify Transform, and the system
	// handles cascading to WorldTransform.
	// term_at(2) matches the *parent's* WorldTransform through a ChildOf cascade traversal.
	// `cascade` walks the ChildOf relationship upward (so `parent_wt` resolves to the nearest
	// ancestor's world transform) AND orders results breadth-first, so a parent is always processed
	// before its children within this system's run. That ordering is what makes the multiply correct:
	// without it, children could read a stale (previous-frame) parent matrix. The pointer type makes
	// the term optional, so unparented/root entities match with `parent_wt == nullptr`.
	std::ignore =
		world.system<const Transform, WorldTransform, const WorldTransform*>("TransformPropagation")
			.term_at(2)
			.cascade(flecs::ChildOf)
			.kind(flecs::PreStore)
			.without<physics::RigidBody>()
			.each([](const Transform& local, WorldTransform& world_transform, const WorldTransform* parent_wt) {
				const glm::mat4 t = glm::translate(glm::mat4(1.0F), local.position);
				const glm::mat4 rx = glm::rotate(glm::mat4(1.0F), local.rotation.x, glm::vec3(1, 0, 0));
				const glm::mat4 ry = glm::rotate(glm::mat4(1.0F), local.rotation.y, glm::vec3(0, 1, 0));
				const glm::mat4 rz = glm::rotate(glm::mat4(1.0F), local.rotation.z, glm::vec3(0, 0, 1));
				const glm::mat4 s = glm::scale(glm::mat4(1.0F), local.scale);
				const glm::mat4 local_matrix = t * rz * ry * rx * s;

				world_transform.matrix = parent_wt ? (parent_wt->matrix * local_matrix) : local_matrix;

				// Extract world-space position, rotation, and scale from the composed matrix
				// so downstream systems (rendering, physics, billboards, culling) have accurate world-space values.
				world_transform.position = glm::vec3(world_transform.matrix[3]);

				// Extract world-space scale (magnitude of each basis axis)
				const glm::vec3 col0{
					world_transform.matrix[0][0],
					world_transform.matrix[1][0],
					world_transform.matrix[2][0]
				};
				const glm::vec3 col1{
					world_transform.matrix[0][1],
					world_transform.matrix[1][1],
					world_transform.matrix[2][1]
				};
				const glm::vec3 col2{
					world_transform.matrix[0][2],
					world_transform.matrix[1][2],
					world_transform.matrix[2][2]
				};
				world_transform.scale.x = glm::length(col0);
				world_transform.scale.y = glm::length(col1);
				world_transform.scale.z = glm::length(col2);

				// Extract world-space rotation by normalizing the rotation matrix (removing scale)
				// and converting back to Euler angles
				const glm::mat3 rot_mat{
					col0 / world_transform.scale.x,
					col1 / world_transform.scale.y,
					col2 / world_transform.scale.z
				};
				world_transform.rotation = glm::eulerAngles(glm::quat_cast(rot_mat));
			})
			.add<scene::GameScene>();
}

} // namespace engine::ecs
