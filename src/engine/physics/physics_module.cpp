#include "physics_module.hpp"

#include <string>

#include <spdlog/spdlog.h>

#include <flecs.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"
#include "engine/scripting/scripting.hpp"
#include "physics_components.hpp"

namespace engine::physics {

namespace {
using json = nlohmann::json;

MotionType ParseMotionType(const std::string& s) {
	if (s == "static") return MotionType::Static;
	if (s == "kinematic") return MotionType::Kinematic;
	return MotionType::Dynamic;
}

ShapeType ParseShapeType(const std::string& s) {
	if (s == "sphere") return ShapeType::Sphere;
	if (s == "capsule") return ShapeType::Capsule;
	if (s == "cylinder") return ShapeType::Cylinder;
	return ShapeType::Box;
}

// Registers the level-loader factories for the physics components this module owns.
void RegisterPhysicsLevelLoaders(const flecs::world& world) {
	using core::JVec3;

	level::RegisterComponentLoader(world, "rigid_body", [](const flecs::entity e, const json& j) {
		RigidBody rb{};
		rb.motion_type = ParseMotionType(j.value("motion_type", std::string{"dynamic"}));
		rb.mass = j.value("mass", rb.mass);
		rb.linear_damping = j.value("linear_damping", rb.linear_damping);
		rb.angular_damping = j.value("angular_damping", rb.angular_damping);
		rb.friction = j.value("friction", rb.friction);
		rb.restitution = j.value("restitution", rb.restitution);
		rb.enable_ccd = j.value("enable_ccd", rb.enable_ccd);
		rb.use_gravity = j.value("use_gravity", rb.use_gravity);
		rb.gravity_scale = j.value("gravity_scale", rb.gravity_scale);
		e.set<RigidBody>(rb);
	});
	level::RegisterComponentLoader(world, "collision_shape", [](const flecs::entity e, const json& j) {
		CollisionShape cs{};
		cs.type = ParseShapeType(j.value("type", std::string{"box"}));
		cs.box_half_extents = JVec3(j, "box_half_extents", cs.box_half_extents);
		cs.sphere_radius = j.value("sphere_radius", cs.sphere_radius);
		cs.capsule_radius = j.value("capsule_radius", cs.capsule_radius);
		cs.capsule_height = j.value("capsule_height", cs.capsule_height);
		cs.offset = JVec3(j, "offset", cs.offset);
		e.set<CollisionShape>(cs);
	});
	level::RegisterComponentLoader(world, "physics_velocity", [](const flecs::entity e, const json& j) {
		PhysicsVelocity v{};
		v.linear = JVec3(j, "linear", v.linear);
		v.angular = JVec3(j, "angular", v.angular);
		e.set<PhysicsVelocity>(v);
	});

	level::RegisterSingletonLoader(world, "physics_world", [](const flecs::world& w, const json& j) {
		auto cfg = w.has<PhysicsWorldConfig>() ? w.get<PhysicsWorldConfig>() : PhysicsWorldConfig{};
		cfg.gravity = JVec3(j, "gravity", cfg.gravity);
		w.set<PhysicsWorldConfig>(cfg);
	});
}
} // namespace

PhysicsModule::PhysicsModule(const flecs::world& world) {
	// Ensure PhysicsWorldConfig singleton exists
	if (!world.has<PhysicsWorldConfig>()) {
		world.set<PhysicsWorldConfig>({});
		world.set<PhysicsConfig>({});
	}

	// === Reflection ===
	world.component<PhysicsImpulse>().member<glm::vec3>("impulse").member<glm::vec3>("point");
	world.component<RigidBody>()
		.member("mass", &RigidBody::mass)
		.member("friction", &RigidBody::friction)
		.member("restitution", &RigidBody::restitution)
		.member("use_gravity", &RigidBody::use_gravity)
		.member("gravity_scale", &RigidBody::gravity_scale);
	world.component<CollisionShape>().member("offset", &CollisionShape::offset);
	world.component<PhysicsVelocity>()
		.member("linear", &PhysicsVelocity::linear)
		.member("angular", &PhysicsVelocity::angular);
	world.component<PhysicsForce>()
		.member("force", &PhysicsForce::force)
		.member("torque", &PhysicsForce::torque)
		.member("clear_after_apply", &PhysicsForce::clear_after_apply);
	world.component<PhysicsVelocityOverride>()
		.member("linear", &PhysicsVelocityOverride::linear)
		.member("angular", &PhysicsVelocityOverride::angular);

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<PhysicsImpulse>());
	scripting::RegisterComponentForScripts(world, world.component<RigidBody>());
	scripting::RegisterComponentForScripts(world, world.component<CollisionShape>());
	scripting::RegisterComponentForScripts(world, world.component<PhysicsVelocity>());
	scripting::RegisterComponentForScripts(world, world.component<PhysicsForce>());
	scripting::RegisterComponentForScripts(world, world.component<PhysicsVelocityOverride>());

	// === Level loader ===
	RegisterPhysicsLevelLoaders(world);

	spdlog::info("[PhysicsModule] Initialized core physics");
}

} // namespace engine::physics
