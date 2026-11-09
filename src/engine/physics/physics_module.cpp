#include "physics_module.hpp"

#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include <flecs.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "engine/assets/assets.hpp"
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
	if (s == "mesh") return ShapeType::Mesh;
	return ShapeType::Box;
}

CollisionLayer ParseCollisionLayer(const std::string& s) {
	if (s == "default") return CollisionLayer::Default;
	if (s == "enemy") return CollisionLayer::Enemy;
	if (s == "obstacle") return CollisionLayer::Obstacle;
	if (s == "trigger") return CollisionLayer::Trigger;
	return CollisionLayer::Player;
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
		cs.mesh_path = assets::ResolveAsset(e.world(), j.value("mesh_path", std::string{}));
		e.set<CollisionShape>(cs);
	});
	level::RegisterComponentLoader(world, "character_body", [](const flecs::entity e, const json& j) {
		CharacterBody body{};
		body.max_slope_deg = j.value("max_slope_deg", body.max_slope_deg);
		body.step_height = j.value("step_height", body.step_height);
		body.stick_to_floor = j.value("stick_to_floor", body.stick_to_floor);
		body.mass = j.value("mass", body.mass);
		body.layer = ParseCollisionLayer(j.value("layer", std::string{"player"}));
		e.set<CharacterBody>(body);
	});
	level::RegisterComponentLoader(world, "gravity", [](const flecs::entity e, const json& j) {
		Gravity g{};
		g.acceleration = JVec3(j, "acceleration", g.acceleration);
		e.set<Gravity>(g);
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
	world.component<ShapeType>()
		.constant("Box", ShapeType::Box)
		.constant("Sphere", ShapeType::Sphere)
		.constant("Capsule", ShapeType::Capsule)
		.constant("Cylinder", ShapeType::Cylinder)
		.constant("ConvexHull", ShapeType::ConvexHull)
		.constant("Mesh", ShapeType::Mesh)
		.constant("Compound", ShapeType::Compound);
	world.component<CollisionShape>()
		.member("type", &CollisionShape::type)
		.member("offset", &CollisionShape::offset)
		.member("mesh_path", &CollisionShape::mesh_path);
	world.component<CharacterBody>()
		.member("max_slope_deg", &CharacterBody::max_slope_deg)
		.member("step_height", &CharacterBody::step_height)
		.member("stick_to_floor", &CharacterBody::stick_to_floor)
		.member("mass", &CharacterBody::mass);
	// CharacterState comes with every CharacterBody, so no observer has to add it mid-iteration.
	world.component<CharacterBody>().add(flecs::With, world.component<CharacterState>());
	world.component<CharacterMove>().member("velocity", &CharacterMove::velocity);
	world.component<Gravity>().member("acceleration", &Gravity::acceleration);
	world.component<CharacterState>()
		.member("grounded", &CharacterState::grounded)
		.member("ground_entity", &CharacterState::ground_entity)
		.member("ground_normal", &CharacterState::ground_normal)
		.member("velocity", &CharacterState::velocity);
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
	// Tags and event components — no meta members, but must be explicitly registered so
	// queries and observers using them never attempt implicit registration during world fini.
	world.component<CollisionEvents>();
	world.component<IsTrigger>();
	world.component<IsSleeping>();

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<PhysicsImpulse>());
	scripting::RegisterComponentForScripts(world, world.component<RigidBody>());
	scripting::RegisterComponentForScripts(world, world.component<CollisionShape>());
	scripting::RegisterComponentForScripts(world, world.component<CharacterBody>());
	scripting::RegisterComponentForScripts(world, world.component<CharacterMove>());
	scripting::RegisterComponentForScripts(world, world.component<Gravity>());
	scripting::RegisterComponentForScripts(world, world.component<CharacterState>());
	scripting::RegisterComponentForScripts(world, world.component<PhysicsVelocity>());
	scripting::RegisterComponentForScripts(world, world.component<PhysicsForce>());
	scripting::RegisterComponentForScripts(world, world.component<PhysicsVelocityOverride>());

	// Reflected enums bind to scripts as ints, so expose ShapeType's values as `Shape_*` constants.
	constexpr std::pair<const char*, ShapeType> k_shape_types[] = {
		{"Shape_Box", ShapeType::Box},
		{"Shape_Sphere", ShapeType::Sphere},
		{"Shape_Capsule", ShapeType::Capsule},
		{"Shape_Cylinder", ShapeType::Cylinder},
		{"Shape_ConvexHull", ShapeType::ConvexHull},
		{"Shape_Mesh", ShapeType::Mesh},
		{"Shape_Compound", ShapeType::Compound},
	};
	std::vector<scripting::ScriptConstant> shape_constants;
	for (const auto& [name, type] : k_shape_types) {
		shape_constants.push_back(
			{.name = name,
			 .type = scripting::ScriptValueType::MakeInt(),
			 .value_str = std::to_string(static_cast<int>(type))}
		);
	}
	scripting::RegisterGlobalConstantsForScripts(world, std::move(shape_constants));

	// === Level loader ===
	RegisterPhysicsLevelLoaders(world);

	spdlog::info("[PhysicsModule] Initialized core physics");
}

} // namespace engine::physics
