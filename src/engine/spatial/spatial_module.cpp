#include "spatial_module.hpp"

#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <spdlog/spdlog.h>

#include "engine/core/core.hpp"
#include "engine/level/level.hpp"
#include "engine/physics/physics.hpp"
#include "engine/scene/scene_components.hpp"
#include "engine/scripting/scripting_module.hpp"

#include "spatial_components.hpp"

namespace engine::spatial {

SpatialModule::SpatialModule(const flecs::world& world) {
	// Reflect components for debugging via the Flecs Explorer and scripting.
	world.component<Transform>().member<glm::vec3>("position").member<glm::vec3>("rotation").member<glm::vec3>("scale");
	world.component<WorldTransform>()
		.member<glm::vec3>("position")
		.member<glm::vec3>("rotation")
		.member<glm::vec3>("scale");
	world.component<Transform>().add(flecs::With, world.component<WorldTransform>());

	scripting::RegisterComponentForScripts(world, world.component<Transform>());
	scripting::RegisterComponentForScripts(world, world.component<WorldTransform>());

	// Register level loader for Transform component.
	level::RegisterComponentLoader(world, "transform", [](const flecs::entity e, const nlohmann::json& j) {
		const glm::vec3 pos = core::JVec3(j, "position", glm::vec3{0.0F});
		const glm::vec3 rot = core::JVec3(j, "rotation", glm::vec3{0.0F});
		const glm::vec3 scale = core::JVec3(j, "scale", glm::vec3{1.0F});
		e.set<Transform>({.position = pos, .rotation = rot, .scale = scale});
	});

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

				glm::quat orientation;
				glm::vec3 skew;
				glm::vec4 perspective;
				glm::decompose(
					world_transform.matrix,
					world_transform.scale,
					orientation,
					world_transform.position,
					skew,
					perspective
				);
				world_transform.rotation = glm::eulerAngles(orientation);
			})
			.add<scene::GameScene>();

	// Set world transform initial value to local transform
	std::ignore = world.observer<WorldTransform>("SetInitialWorldTransform")
					  .event(flecs::OnAdd)
					  .each([](const flecs::entity e, WorldTransform& wt) {
						  if (e.has<Transform>()) {
							  const auto& [position, rotation, scale] = e.get<Transform>();
							  wt.position = position;
							  wt.rotation = rotation;
							  wt.scale = scale;
							  wt.ComputeMatrix();
						  }
					  });

	spdlog::info("[SpatialModule] Registered spatial and transform systems with Flecs");
}

} // namespace engine::spatial
