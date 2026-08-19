#include "level_module.hpp"

#include <cstdint>
#include <string>

#include <spdlog/spdlog.h>

#include <flecs.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "engine/assets/assets.hpp"
#include "engine/audio/audio.hpp"
#include "engine/core/json_util.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/physics/physics.hpp"
#include "engine/platform/platform.hpp"
#include "engine/render/render.hpp"
#include "engine/scripting/scripting.hpp"
#include "level_components.hpp"

namespace engine::level {

namespace {

using json = nlohmann::json;

// JSON field parsers are centralised in engine/core/json_util.hpp; alias them so existing call
// sites (JVec3/JVec2/JRgba) keep working without duplicating the parsing logic here.
using core::JRgba;
using core::JVec2;
using core::JVec3;

physics::MotionType ParseMotionType(const std::string& s) {
	if (s == "static") {
		return physics::MotionType::Static;
	}
	if (s == "kinematic") {
		return physics::MotionType::Kinematic;
	}
	return physics::MotionType::Dynamic;
}

physics::ShapeType ParseShapeType(const std::string& s) {
	if (s == "sphere") {
		return physics::ShapeType::Sphere;
	}
	if (s == "capsule") {
		return physics::ShapeType::Capsule;
	}
	if (s == "cylinder") {
		return physics::ShapeType::Cylinder;
	}
	return physics::ShapeType::Box;
}

// === Built-in component loaders ===

// Register the loaders for the engine's own components. Each is keyed by the JSON component name.
void RegisterBuiltinLoaders(LevelRegistry& reg) {
	// transform: also computes the WorldTransform so static entities render/collide correctly
	// without the author having to duplicate the values.
	reg.loaders["transform"] = [](const flecs::entity e, const json& j) {
		const glm::vec3 pos = JVec3(j, "position", glm::vec3{0.0F});
		const glm::vec3 rot = JVec3(j, "rotation", glm::vec3{0.0F});
		const glm::vec3 scale = JVec3(j, "scale", glm::vec3{1.0F});
		e.set<ecs::Transform>({pos, rot, scale});
		ecs::WorldTransform wt{pos, rot, scale};
		wt.ComputeMatrix();
		e.set<ecs::WorldTransform>(wt);
	};

	reg.loaders["camera"] = [](const flecs::entity e, const json& j) {
		render::Camera c{};
		c.target = JVec3(j, "target", c.target);
		c.up = JVec3(j, "up", c.up);
		c.fov = j.value("fov", c.fov);
		c.aspect_ratio = j.value("aspect_ratio", c.aspect_ratio);
		c.near_plane = j.value("near", c.near_plane);
		c.far_plane = j.value("far", c.far_plane);
		e.set<render::Camera>(c);
	};

	reg.loaders["cube"] = [](const flecs::entity e, const json& j) {
		e.set<render::CubePrimitive>({JVec3(j, "size", glm::vec3{1.0F})});
	};
	reg.loaders["sphere"] = [](const flecs::entity e, const json& j) {
		e.set<render::SpherePrimitive>({j.value("radius", 0.5F)});
	};
	reg.loaders["quad"] = [](const flecs::entity e, const json& j) {
		e.set<render::QuadPrimitive>({JVec2(j, "size", glm::vec2{1.0F})});
	};
	reg.loaders["capsule"] = [](const flecs::entity e, const json& j) {
		e.set<render::CapsulePrimitive>({j.value("radius", 0.5F), j.value("height", 1.0F)});
	};
	reg.loaders["mesh"] = [](const flecs::entity e, const json& j) {
		e.set<render::MeshPrimitive>({.path = assets::ResolveAsset(e.world(), j.value("path", std::string{}))});
	};

	reg.loaders["material"] = [](const flecs::entity e, const json& j) {
		render::Material m{};
		m.color = JRgba(j, "color", m.color);
		m.wireframe = j.value("wireframe", m.wireframe);
		m.cast_shadow = j.value("cast_shadow", m.cast_shadow);
		e.set<render::Material>(m);
	};
	reg.loaders["albedo"] = [](const flecs::entity e, const json& j) {
		e.set<render::AlbedoMap>({.path = assets::ResolveAsset(e.world(), j.value("path", std::string{}))});
	};

	reg.loaders["directional_light"] = [](const flecs::entity e, const json& j) {
		render::DirectionalLight dl{};
		dl.direction = JVec3(j, "direction", dl.direction);
		dl.color = JRgba(j, "color", dl.color);
		dl.intensity = j.value("intensity", dl.intensity);
		dl.specular_strength = j.value("specular_strength", dl.specular_strength);
		dl.shininess = j.value("shininess", dl.shininess);
		dl.casts_shadows = j.value("casts_shadows", dl.casts_shadows);
		dl.shadow_ground_y = j.value("shadow_ground_y", dl.shadow_ground_y);
		dl.shadow_color = JRgba(j, "shadow_color", dl.shadow_color);
		e.set<render::DirectionalLight>(dl);
	};

	reg.loaders["rigid_body"] = [](const flecs::entity e, const json& j) {
		physics::RigidBody rb{};
		rb.motion_type = ParseMotionType(j.value("motion_type", std::string{"dynamic"}));
		rb.mass = j.value("mass", rb.mass);
		rb.linear_damping = j.value("linear_damping", rb.linear_damping);
		rb.angular_damping = j.value("angular_damping", rb.angular_damping);
		rb.friction = j.value("friction", rb.friction);
		rb.restitution = j.value("restitution", rb.restitution);
		rb.enable_ccd = j.value("enable_ccd", rb.enable_ccd);
		rb.use_gravity = j.value("use_gravity", rb.use_gravity);
		rb.gravity_scale = j.value("gravity_scale", rb.gravity_scale);
		e.set<physics::RigidBody>(rb);
	};
	reg.loaders["collision_shape"] = [](const flecs::entity e, const json& j) {
		physics::CollisionShape cs{};
		cs.type = ParseShapeType(j.value("type", std::string{"box"}));
		cs.box_half_extents = JVec3(j, "box_half_extents", cs.box_half_extents);
		cs.sphere_radius = j.value("sphere_radius", cs.sphere_radius);
		cs.capsule_radius = j.value("capsule_radius", cs.capsule_radius);
		cs.capsule_height = j.value("capsule_height", cs.capsule_height);
		cs.offset = JVec3(j, "offset", cs.offset);
		e.set<physics::CollisionShape>(cs);
	};
	reg.loaders["physics_velocity"] = [](const flecs::entity e, const json& j) {
		physics::PhysicsVelocity v{};
		v.linear = JVec3(j, "linear", v.linear);
		v.angular = JVec3(j, "angular", v.angular);
		e.set<physics::PhysicsVelocity>(v);
	};

	reg.loaders["sound_effect"] = [](const flecs::entity e, const json& j) {
		e.set<audio::SoundEffect>({.path = assets::ResolveAsset(e.world(), j.value("path", std::string{}))});
	};

	reg.loaders["script"] = [](const flecs::entity e, const json& j) {
		e.set<scripting::ScriptComponent>(
			{.source_path = assets::ResolveAsset(e.world(), j.value("source", std::string{}))}
		);
	};
}

// Register the loaders for world singletons.
void RegisterBuiltinSingletonLoaders(LevelRegistry& reg) {
	reg.singletonLoaders["ambient_light"] = [](const flecs::world& world, const json& j) {
		render::AmbientLight al{};
		al.color = JRgba(j, "color", al.color);
		al.intensity = j.value("intensity", al.intensity);
		world.set<render::AmbientLight>(al);
	};
	reg.singletonLoaders["physics_world"] = [](const flecs::world& world, const json& j) {
		auto cfg = world.has<physics::PhysicsWorldConfig>() ? world.get<physics::PhysicsWorldConfig>()
															: physics::PhysicsWorldConfig{};
		cfg.gravity = JVec3(j, "gravity", cfg.gravity);
		world.set<physics::PhysicsWorldConfig>(cfg);
	};
}

// Instantiate one entity definition (and its children) into the world.
flecs::entity BuildEntity(const flecs::world& world, const json& def, const flecs::entity parent) {
	flecs::entity e = def.contains("name") ? world.entity(def.at("name").get<std::string>().c_str()) : world.entity();
	if (parent) {
		e.child_of(parent);
	}
	if (def.contains("components")) {
		const auto& reg = world.get<LevelRegistry>();
		for (const auto& [key, value] : def.at("components").items()) {
			const auto it = reg.loaders.find(key);
			if (it == reg.loaders.end()) {
				spdlog::warn("[Level] Unknown component '{}' on entity '{}'", key, e.name().c_str());
				continue;
			}
			it->second(e, value);
		}
	}
	if (def.contains("children")) {
		for (const auto& child : def.at("children")) {
			BuildEntity(world, child, e);
		}
	}
	return e;
}

} // namespace

LevelModule::LevelModule(const flecs::world& world) {
	world.set<LevelRegistry>({});
	auto& reg = world.get_mut<LevelRegistry>();
	RegisterBuiltinLoaders(reg);
	RegisterBuiltinSingletonLoaders(reg);
	spdlog::info("[LevelModule] Registered level registry with built-in component loaders");
}

void RegisterComponentLoader(const flecs::world& world, const std::string_view name, ComponentLoader loader) {
	if (!world.has<LevelRegistry>()) {
		return;
	}
	world.get_mut<LevelRegistry>().loaders[std::string{name}] = std::move(loader);
}

void RegisterSingletonLoader(const flecs::world& world, const std::string_view name, SingletonLoader loader) {
	if (!world.has<LevelRegistry>()) {
		return;
	}
	world.get_mut<LevelRegistry>().singletonLoaders[std::string{name}] = std::move(loader);
}

int LoadLevel(const flecs::world& world, const std::string_view path) {
	if (!world.has<LevelRegistry>()) {
		spdlog::error("[Level] LevelRegistry not set — import LevelModule before loading a level");
		return -1;
	}

	const auto doc_opt = core::LoadJsonFile(path, "Level");
	if (!doc_opt) {
		return -1;
	}
	const json& doc = *doc_opt;

	if (doc.contains("singletons")) {
		const auto& reg = world.get<LevelRegistry>();
		for (const auto& [key, value] : doc.at("singletons").items()) {
			const auto it = reg.singletonLoaders.find(key);
			if (it == reg.singletonLoaders.end()) {
				spdlog::warn("[Level] Unknown singleton '{}'", key);
				continue;
			}
			it->second(world, value);
		}
	}

	int count = 0;
	if (doc.contains("entities")) {
		for (const auto& def : doc.at("entities")) {
			BuildEntity(world, def, flecs::entity{});
			++count;
		}
	}
	spdlog::info("[Level] Loaded '{}' ({} top-level entities)", path, count);
	return count;
}

} // namespace engine::level
