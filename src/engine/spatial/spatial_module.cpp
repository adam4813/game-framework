#include "spatial_module.hpp"

#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>

#include "engine/core/core.hpp"
#include "engine/level/level.hpp"
#include "engine/physics/physics.hpp"
#include "engine/scene/scene_components.hpp"
#include "engine/scripting/scripting_module.hpp"

#include "engine/ecs/pausable.hpp"
#include "spatial_components.hpp"

namespace engine::spatial {

void ComposeWorldTransform(const Transform& local, const WorldTransform* parent_wt, WorldTransform& world_transform) {
	// Compose the local matrix. The overwhelmingly common case is an unrotated entity, so
	// skip the four glm::rotate calls (each a sin/cos) and their matrix products entirely
	// when there is no local rotation — this is the hot path for scenes with tens of
	// thousands of moving entities.
	glm::mat4 local_matrix;
	if (local.rotation.x == 0.0F && local.rotation.y == 0.0F && local.rotation.z == 0.0F) {
		local_matrix = glm::mat4(1.0F);
		local_matrix[0][0] = local.scale.x;
		local_matrix[1][1] = local.scale.y;
		local_matrix[2][2] = local.scale.z;
		local_matrix[3] = glm::vec4(local.position, 1.0F);
	}
	else {
		const glm::mat4 t = glm::translate(glm::mat4(1.0F), local.position);
		const glm::mat4 rx = glm::rotate(glm::mat4(1.0F), local.rotation.x, glm::vec3(1, 0, 0));
		const glm::mat4 ry = glm::rotate(glm::mat4(1.0F), local.rotation.y, glm::vec3(0, 1, 0));
		const glm::mat4 rz = glm::rotate(glm::mat4(1.0F), local.rotation.z, glm::vec3(0, 0, 1));
		const glm::mat4 s = glm::scale(glm::mat4(1.0F), local.scale);
		local_matrix = t * rz * ry * rx * s;
	}

	// Root entity (no parent WorldTransform): world == local, so copy the authored
	// components directly. No matrix decomposition is needed — this is the fast path that
	// avoids glm::decompose for every unparented entity (the common case).
	if (parent_wt == nullptr) {
		world_transform.matrix = local_matrix;
		world_transform.position = local.position;
		world_transform.rotation = local.rotation;
		world_transform.scale = local.scale;
		return;
	}

	world_transform.matrix = parent_wt->matrix * local_matrix;

	// Fast path: when neither the local entity nor its parent introduces rotation, the world
	// transform is a pure translate+scale — skip the sqrt/quat_cast/eulerAngles decomposition
	// below and derive the components directly. This is the hot path for large numbers of
	// unrotated agents parented to an unrotated scene root.
	if (local.rotation.x == 0.0F && local.rotation.y == 0.0F && local.rotation.z == 0.0F &&
		parent_wt->rotation.x == 0.0F && parent_wt->rotation.y == 0.0F && parent_wt->rotation.z == 0.0F) {
		world_transform.position = glm::vec3(world_transform.matrix[3]);
		world_transform.scale = parent_wt->scale * local.scale;
		world_transform.rotation = glm::vec3(0.0F);
		return;
	}

	// Parented entity: extract world-space components from the composed matrix. This is a
	// lightweight, affine-only substitute for glm::decompose (which additionally solves for
	// skew and perspective that these transforms never contain).
	world_transform.position = glm::vec3(world_transform.matrix[3]);
	const glm::vec3 c0(world_transform.matrix[0]);
	const glm::vec3 c1(world_transform.matrix[1]);
	const glm::vec3 c2(world_transform.matrix[2]);
	const float sx = glm::length(c0);
	const float sy = glm::length(c1);
	const float sz = glm::length(c2);
	world_transform.scale = glm::vec3(sx, sy, sz);
	const glm::mat3 rotation_matrix{
		sx > 0.0F ? c0 / sx : glm::vec3(1, 0, 0),
		sy > 0.0F ? c1 / sy : glm::vec3(0, 1, 0),
		sz > 0.0F ? c2 / sz : glm::vec3(0, 0, 1),
	};
	world_transform.rotation = glm::eulerAngles(glm::quat_cast(rotation_matrix));
}

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
				ComposeWorldTransform(local, parent_wt, world_transform);
			})
			.add<scene::GameScene>()
			.add<ecs::Pausable>();

	// === OBSERVER: Seed WorldTransform whenever Transform is set ===
	// OnSet, not OnAdd: outside a deferred merge (an undeferred scene Load, a script), set<Transform>
	// adds the component default-constructed and fires OnAdd before the value is copied in, so an
	// OnAdd seed would read the defaults and leave the entity at the origin. Rigid bodies are skipped
	// by TransformPropagation, so this seed is their only Transform -> WorldTransform path: setting a
	// rigid body's Transform teleports it (PhysicsSetBodyTransform reacts to the WorldTransform set).
	std::ignore = world.observer<const Transform>("TransformSeedWorldTransform")
					  .event(flecs::OnSet)
					  .each([](const flecs::entity e, const Transform& local) {
						  const flecs::entity parent = e.parent();
						  const auto* parent_wt = parent ? parent.try_get<WorldTransform>() : nullptr;
						  WorldTransform world_transform;
						  ComposeWorldTransform(local, parent_wt, world_transform);
						  e.set<WorldTransform>(world_transform);
					  });

	spdlog::info("[SpatialModule] Registered spatial and transform systems with Flecs");
}

} // namespace engine::spatial
