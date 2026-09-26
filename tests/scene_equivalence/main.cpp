#include "null_platform.hpp"
#include "scene_data.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <flecs.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/engine.hpp"

namespace {

using json = nlohmann::json;

// TestWorld wraps the full EngineContext with NullPlatform — same engine stack as the game,
// no window or GPU required. The level loader uses nlohmann::ordered_json so JSON component
// keys are processed in file order (transform before rigid_body), ensuring Jolt observers
// fire after WorldTransform is already seeded.
struct TestWorld {
	std::unique_ptr<engine::EngineContext> ctx;

	TestWorld() {
		ctx = std::make_unique<engine::EngineContext>(std::make_unique<NullPlatform>());
		ctx->GetModuleRegistry().Import(
			*ctx->GetWorld(),
			{"ai", "physics", "noise", "pathfinding", "save", "tilemap", "particles"}
		);
	}

	flecs::world& World() const { return *ctx->GetWorld(); }
};

struct DiffRow {
	std::string key;
	std::string json_value;
	std::string as_value;
	std::string cpp_value;
	bool mismatch{false};
};

std::string EntityName(const flecs::entity entity) {
	const char* name = entity.name();
	return (name && *name != '\0') ? name : std::to_string(entity.id());
}

json SerializeEntity(const flecs::entity entity) {
	flecs::entity_to_json_desc_t desc = ECS_ENTITY_TO_JSON_INIT;
	desc.serialize_full_paths = false;
	desc.serialize_values = true;
	desc.serialize_builtin = false;

	const flecs::string serialized = entity.to_json(&desc);
	return json::parse(serialized.c_str());
}

json SerializeSceneRoot(flecs::world& world, const flecs::entity scene_root) {
	std::vector<flecs::entity> children;
	scene_root.children([&](const flecs::entity child) {
		if (child.has<engine::scripting::ScriptComponent>()) {
			return;
		}
		children.push_back(child);
	});

	std::sort(children.begin(), children.end(), [](const flecs::entity lhs, const flecs::entity rhs) {
		return EntityName(lhs) < EntityName(rhs);
	});

	json child_json = json::object();
	for (const flecs::entity child : children) {
		const std::string name = EntityName(child);
		child_json[name] = SerializeEntity(child);
	}

	return {
		{"scene_root", EntityName(scene_root)},
		{"children", child_json},
	};
}

void FlattenJsonValue(const json& value, const std::string& prefix, std::map<std::string, json>& out) {
	if (value.is_object()) {
		for (const auto& [key, child] : value.items()) {
			if (key == "handle") {
				continue;
			}
			const std::string child_prefix = prefix.empty() ? key : prefix + "." + key;
			FlattenJsonValue(child, child_prefix, out);
		}
		return;
	}

	out[prefix] = value;
}

std::map<std::string, json> FlattenScene(const json& scene_json) {
	std::map<std::string, json> flattened;
	if (!scene_json.contains("children")) {
		return flattened;
	}

	for (const auto& [entity_name, entity_json] : scene_json.at("children").items()) {
		if (!entity_json.contains("components")) {
			continue;
		}
		for (const auto& [component_name, component_value] : entity_json.at("components").items()) {
			if (component_name == "WorldTransform") {
				continue;
			}
			// Skip components with no meta (serialized as null by Flecs) — no values to compare.
			if (component_value.is_null()) {
				continue;
			}
			FlattenJsonValue(component_value, entity_name + "#" + component_name, flattened);
		}
	}

	return flattened;
}

std::string FormatJsonValue(const json& value) {
	if (value.is_number_float()) {
		std::ostringstream stream;
		stream << std::fixed << std::setprecision(6) << value.get<double>();
		return stream.str();
	}
	if (value.is_boolean()) {
		return value.get<bool>() ? "true" : "false";
	}
	if (value.is_number_integer()) {
		return std::to_string(value.get<std::int64_t>());
	}
	if (value.is_number_unsigned()) {
		return std::to_string(value.get<std::uint64_t>());
	}
	if (value.is_string()) {
		return value.get<std::string>();
	}
	if (value.is_null()) {
		return "null";
	}
	return value.dump();
}

bool ValuesEqual(const json& lhs, const json& rhs, const double epsilon) {
	if (lhs.type() != rhs.type()) {
		if (lhs.is_number() && rhs.is_number()) {
			return std::abs(lhs.get<double>() - rhs.get<double>()) <= epsilon;
		}
		return false;
	}

	if (lhs.is_number_float() && rhs.is_number_float()) {
		return std::abs(lhs.get<double>() - rhs.get<double>()) <= epsilon;
	}

	return lhs == rhs;
}

std::vector<DiffRow>
ComputeDiff(const json& json_world, const json& as_world, const json& cpp_world, const double epsilon = 1e-4) {
	const auto flattened_json = FlattenScene(json_world);
	const auto flattened_as = FlattenScene(as_world);
	const auto flattened_cpp = FlattenScene(cpp_world);

	std::set<std::string> all_keys;
	for (const auto& [key, _] : flattened_json) {
		all_keys.insert(key);
	}
	for (const auto& [key, _] : flattened_as) {
		all_keys.insert(key);
	}
	for (const auto& [key, _] : flattened_cpp) {
		all_keys.insert(key);
	}

	std::vector<DiffRow> rows;
	rows.reserve(all_keys.size());

	for (const auto& key : all_keys) {
		const auto json_it = flattened_json.find(key);
		const auto as_it = flattened_as.find(key);
		const auto cpp_it = flattened_cpp.find(key);

		const bool has_json = json_it != flattened_json.end();
		const bool has_as = as_it != flattened_as.end();
		const bool has_cpp = cpp_it != flattened_cpp.end();

		bool mismatch = !(has_json && has_as && has_cpp);
		if (!mismatch) {
			mismatch = !ValuesEqual(json_it->second, as_it->second, epsilon)
					   || !ValuesEqual(json_it->second, cpp_it->second, epsilon)
					   || !ValuesEqual(as_it->second, cpp_it->second, epsilon);
		}

		rows.push_back({
			.key = key,
			.json_value = has_json ? FormatJsonValue(json_it->second) : "N/A",
			.as_value = has_as ? FormatJsonValue(as_it->second) : "N/A",
			.cpp_value = has_cpp ? FormatJsonValue(cpp_it->second) : "N/A",
			.mismatch = mismatch,
		});
	}

	return rows;
}

void WriteJsonFile(const std::filesystem::path& path, const json& value) {
	std::ofstream out(path, std::ios::binary);
	if (!out) {
		throw std::runtime_error("Failed to open output file: " + path.string());
	}
	out << value.dump(2) << '\n';
}

void WriteDiffTable(const std::filesystem::path& path, const std::vector<DiffRow>& rows) {
	const std::string key_header = "entity#Component.field";
	const std::string json_header = "JSON";
	const std::string as_header = "AS";
	const std::string cpp_header = "C++";

	size_t key_width = key_header.size();
	size_t json_width = json_header.size();
	size_t as_width = as_header.size();
	size_t cpp_width = cpp_header.size();
	size_t match_width = 0;

	for (const auto& row : rows) {
		key_width = std::max(key_width, row.key.size());
		json_width = std::max(json_width, row.json_value.size());
		as_width = std::max(as_width, row.as_value.size());
		cpp_width = std::max(cpp_width, row.cpp_value.size());
		match_width = std::max(
			match_width,
			row.mismatch ? std::string_view("[MISMATCH]").size() + 1 : std::string_view("[MATCH]").size() + 1
		);
	}

	auto separator = [](const size_t width) { return std::string(width + 2, '-'); };

	std::ofstream out(path, std::ios::binary);
	if (!out) {
		throw std::runtime_error("Failed to open diff table: " + path.string());
	}

	out
		<< "| "
		<< std::left
		<< std::setw(static_cast<int>(key_width))
		<< key_header
		<< " | "
		<< std::setw(static_cast<int>(json_width))
		<< json_header
		<< " | "
		<< std::setw(static_cast<int>(as_width))
		<< as_header
		<< " | "
		<< std::setw(static_cast<int>(cpp_width))
		<< cpp_header
		<< " | "
		<< std::setw(static_cast<int>(match_width - 1))
		<< ""
		<< " |\n";
	out
		<< "|"
		<< separator(key_width)
		<< "|"
		<< separator(json_width)
		<< "|"
		<< separator(as_width)
		<< "|"
		<< separator(cpp_width)
		<< "|"
		<< separator(match_width - 1)
		<< "|\n";

	for (const auto& row : rows) {
		out
			<< "| "
			<< std::setw(static_cast<int>(key_width))
			<< row.key
			<< " | "
			<< std::setw(static_cast<int>(json_width))
			<< row.json_value
			<< " | "
			<< std::setw(static_cast<int>(as_width))
			<< row.as_value
			<< " | "
			<< std::setw(static_cast<int>(cpp_width))
			<< row.cpp_value
			<< " |"
			<< std::setw(static_cast<int>(match_width))
			<< (row.mismatch ? " [MISMATCH]" : " [MATCH]")
			<< " |\n";
	}
}

json BuildCppWorld() {
	TestWorld test_world;
	const flecs::entity scene_root = test_world.World().entity("SceneRoot");

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestCamera")
		.set<engine::spatial::Transform>({
			.position = {0.0F, 5.0F, 5.0F},
			.rotation = {0.0F, 0.0F, 0.0F},
			.scale = {1.0F, 1.0F, 1.0F},
		})
		.set<engine::render::Camera>({
			.up = {0.0F, 1.0F, 0.0F},
			.fov = 60.0F,
			.aspect_ratio = 1.7777778F,
			.near_plane = 0.1F,
			.far_plane = 100.0F,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestLight")
		.set<engine::spatial::Transform>({
			.position = {0.0F, 10.0F, 0.0F},
			.rotation = {0.0F, 0.0F, 0.0F},
			.scale = {1.0F, 1.0F, 1.0F},
		})
		.set<engine::render::DirectionalLight>({
			.direction = {-0.55F, -1.0F, -0.4F},
			.color = {.r = 255, .g = 245, .b = 220, .a = 255},
			.intensity = 1.0F,
			.specular_strength = 0.35F,
			.shininess = 24.0F,
			.casts_shadows = true,
			.shadow_ground_y = 0.0F,
			.shadow_color = {.r = 8, .g = 10, .b = 14, .a = 115},
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestBox")
		.set<engine::spatial::Transform>({
			.position = {0.0F, 3.0F, 0.0F},
			.rotation = {0.0F, 0.0F, 0.0F},
			.scale = {1.0F, 1.0F, 1.0F},
		})
		.set<engine::render::CubePrimitive>({
			.size = {1.0F, 1.0F, 1.0F},
		})
		// CollisionShape MUST be set before RigidBody so the Jolt OnSet<RigidBody> observer
		// can read it. The observer calls e.get<CollisionShape>() synchronously on OnSet.
		.set<engine::physics::CollisionShape>({
			.type = engine::physics::ShapeType::Box,
			.box_half_extents = {0.5F, 0.5F, 0.5F},
		})
		.set<engine::physics::RigidBody>({
			.motion_type = engine::physics::MotionType::Dynamic,
			.mass = 1.0F,
			.linear_damping = 0.05F,
			.angular_damping = 0.05F,
			.friction = 0.5F,
			.restitution = 0.3F,
			.enable_ccd = false,
			.use_gravity = true,
			.gravity_scale = 1.0F,
			.layer = engine::physics::CollisionLayer::Default,
			.collision_mask = 0xFFFFFFFFu,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestSphere")
		.set<engine::spatial::Transform>({
			.position = {-2.5F, 0.5F, 0.0F},
			.rotation = {0.0F, 0.0F, 0.0F},
			.scale = {1.0F, 1.0F, 1.0F},
		})
		.set<engine::render::SpherePrimitive>({
			.radius = 0.5F,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestTimer")
		.set<engine::timer::Timer>({
			.remaining = 5.0F,
			.duration = 5.0F,
			.repeat = false,
			.expired = false,
		})
		.set<engine::timer::Cooldown>({
			.remaining = 2.0F,
			.duration = 2.0F,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestGraphNode")
		.set<engine::graph::GraphNode>({
			.label = "MainNode",
			.order = 1,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestGraphEdge")
		.set<engine::graph::GraphEdgeWeight>({
			.weight = 0.75F,
			.tag = "strong",
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestSteering")
		.set<engine::spatial::Transform>({
			.position = {7.0F, 0.0F, 0.0F},
			.rotation = {0.0F, 0.0F, 0.0F},
			.scale = {1.0F, 1.0F, 1.0F},
		})
		.set<engine::ai::SteeringAgent>({
			.max_speed = 5.0F,
			.max_force = 10.0F,
			.mass = 1.5F,
		})
		.set<engine::ai::SteeringTarget>({
			.position = {1.0F, 0.0F, 3.0F},
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestFSMAgent")
		.set<engine::ai::FSMAgent>({
			.current_state = "idle",
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestGOAPAgent")
		.set<engine::ai::GOAPAgent>({
			.needs_replan = true,
			.plan_failed = false,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestBTree")
		.set<engine::ai::BehaviorTree>({
			.tick_interval = 0.1F,
		});

	test_world.World().entity().child_of(scene_root).set_name("TestUtility").set<engine::ai::UtilityAgent>({});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestGrid")
		.set<engine::tilemap::GridPosition>({.x = 2, .z = 3});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestQuad")
		.set<engine::spatial::Transform>({
			.position = {3.0F, 0.0F, 0.0F},
			.rotation = {0.0F, 0.0F, 0.0F},
			.scale = {1.0F, 1.0F, 1.0F},
		})
		.set<engine::render::QuadPrimitive>({
			.size = {2.0F, 3.0F},
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestCapsule")
		.set<engine::spatial::Transform>({
			.position = {5.0F, 0.0F, 0.0F},
			.rotation = {0.0F, 0.0F, 0.0F},
			.scale = {1.0F, 1.0F, 1.0F},
		})
		.set<engine::render::CapsulePrimitive>({
			.radius = 0.4F,
			.height = 1.5F,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestParticles")
		.set<engine::spatial::Transform>({
			.position = {0.0F, 0.0F, 5.0F},
			.rotation = {0.0F, 0.0F, 0.0F},
			.scale = {1.0F, 1.0F, 1.0F},
		})
		.set<engine::particles::ParticleEmitter>({
			.rate = 30.0F,
			.accumulator = 0.0F,
			.particleLifetime = 0.6F,
			.speed = 2.5F,
			.startSize = 0.08F,
			.spread = 0.6F,
			.direction = {0.0F, 1.0F, 0.0F},
			.colorStart = {.r = 255, .g = 200, .b = 80, .a = 255},
			.colorEnd = {.r = 255, .g = 80, .b = 30, .a = 0},
			.emitting = false,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestTween")
		.set<engine::timer::Tween>({
			.elapsed = 0.0F,
			.duration = 2.0F,
			.from = 0.0F,
			.to = 1.0F,
			.easing = engine::timer::EasingType::Linear,
			.value = 0.0F,
			.done = false,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestMaterial")
		.set<engine::render::Material>({
			.color = {.r = 200, .g = 100, .b = 50, .a = 255},
			.wireframe = false,
			.cast_shadow = true,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestVelocity")
		.set<engine::spatial::Transform>({
			.position = {0.0F, 0.0F, -3.0F},
			.rotation = {0.0F, 0.0F, 0.0F},
			.scale = {1.0F, 1.0F, 1.0F},
		})
		.set<engine::physics::PhysicsVelocity>({
			.linear = {1.0F, 0.0F, 0.0F},
			.angular = {0.0F, 0.5F, 0.0F},
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestShape")
		.set<engine::spatial::Transform>({
			.position = {-3.0F, 0.0F, 0.0F},
			.rotation = {0.0F, 0.0F, 0.0F},
			.scale = {1.0F, 1.0F, 1.0F},
		})
		.set<engine::physics::CollisionShape>({
			.offset = {0.0F, 0.1F, 0.0F},
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestAudio")
		.set<engine::audio::SoundEffect>({
			.path = engine::assets::ResolveAsset(test_world.World(), "audio/sfx/jump.wav"),
			.playing = false,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestMesh")
		.set<engine::render::MeshPrimitive>({
			.path = engine::assets::ResolveAsset(test_world.World(), "models/cube.glb"),
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestAlbedo")
		.set<engine::render::AlbedoMap>({
			.path = engine::assets::ResolveAsset(test_world.World(), "textures/checker.png"),
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestMusic")
		.set<engine::audio::Music>({
			.path = engine::assets::ResolveAsset(test_world.World(), "audio/music/theme.ogg"),
			.loop = true,
		});

	test_world.World().entity().child_of(scene_root).set_name("TestTilemapRegistry").set<engine::tilemap::TileSet>({});

	const flecs::entity atlas_ent = test_world.World()
										.entity()
										.child_of(scene_root)
										.set_name("TestTilemapAtlas")
										.set<engine::render::SpriteAtlas>({
											.texture_path = "./textures/tileset.png",
											.image_width_px = 128,
											.image_height_px = 128,
										})
										.set<engine::render::Material>({
											.color = {.r = 255, .g = 255, .b = 255, .a = 255},
											.cast_shadow = true,
										});

	std::vector<uint32_t> tile_ids(100, 0);
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestTilemap")
		.set<engine::tilemap::Tilemap>({
			.width = 10,
			.height = 10,
			.tile_size_px = 16,
			.pixels_per_world_unit = 32.0f,
			.tile_ids = tile_ids,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestPathAgent")
		.set<engine::pathfinding::PathAgent>({
			.arrival_radius = 0.5F,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestPathFollow")
		.set<engine::pathfinding::PathFollow>({
			.speed = 4.0F,
		});

	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestPathAutoReplan")
		.set<engine::pathfinding::PathAutoReplan>({
			.goal = {3.0F, 0.0F, 5.0F},
			.interval = 0.5F,
		});

	// --- UI (script-registered components authorable in all three paths) ---
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUIPanel")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 20.0F, .w = 200.0F, .h = 100.0F}})
		.set<engine::ui::UIElement>({.z_index = 5, .visible = true})
		.set<engine::ui::Panel>({
			.color = {.r = 40, .g = 46, .b = 40, .a = 255},
			.border_color = {.r = 90, .g = 100, .b = 90, .a = 255},
			.border_thickness = 2.0F,
			.roundness = 0.1F,
		});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUIButton")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 140.0F, .w = 160.0F, .h = 48.0F}})
		.set<engine::ui::Button>({
			.label = "OK",
			.font_size = 22.0F,
			.normal = {.r = 70, .g = 140, .b = 70, .a = 255},
			.hover = {.r = 56, .g = 64, .b = 56, .a = 255},
			.pressed_color = {.r = 40, .g = 46, .b = 40, .a = 255},
			.border_color = {.r = 90, .g = 100, .b = 90, .a = 255},
			.text_color = {.r = 220, .g = 225, .b = 220, .a = 255},
			.roundness = 0.25F,
		});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUIProgressBar")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 200.0F, .w = 200.0F, .h = 24.0F}})
		.set<engine::ui::ProgressBar>({
			.value = 0.6F,
			.min = 0.0F,
			.max = 1.0F,
			.fill_color = {.r = 70, .g = 140, .b = 70, .a = 255},
			.track_color = {.r = 18, .g = 20, .b = 18, .a = 235},
			.border_color = {.r = 90, .g = 100, .b = 90, .a = 255},
			.border_thickness = 2.0F,
			.roundness = 0.35F,
		});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUISpinner")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 240.0F, .w = 48.0F, .h = 48.0F}})
		.set<engine::ui::Spinner>({
			.color = {.r = 70, .g = 140, .b = 70, .a = 255},
			.radius = 18.0F,
			.dot_radius = 4.0F,
			.dots = 8,
			.speed = 6.0F,
		});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUIStack")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 300.0F, .w = 200.0F, .h = 100.0F}})
		.set<engine::ui::Stack>({.spacing = 8.0F, .padding = 4.0F, .stretch = true});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUIScrollRect")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 420.0F, .w = 200.0F, .h = 150.0F}})
		.set<engine::ui::ScrollRect>({
			.vertical = true,
			.horizontal = false,
			.scrollbar_thickness = 10.0F,
			.wheel_speed = 28.0F,
			.track_color = {.r = 18, .g = 20, .b = 18, .a = 235},
			.thumb_color = {.r = 56, .g = 64, .b = 56, .a = 255},
		});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUIModal")
		.set<engine::ui::UIRect>({{.x = 300.0F, .y = 100.0F, .w = 400.0F, .h = 300.0F}})
		.set<engine::ui::Modal>({
			.open = true,
			.backdrop_color = {.r = 6, .g = 8, .b = 6, .a = 200},
			.close_on_backdrop = false,
		});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUILabel")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 580.0F, .w = 200.0F, .h = 28.0F}})
		.set<engine::ui::Label>({
			.text = "Hello",
			.font_size = 24.0F,
			.color = {.r = 200, .g = 210, .b = 200, .a = 255},
		});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUISlider")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 620.0F, .w = 200.0F, .h = 24.0F}})
		.set<engine::ui::Slider>({.value = 0.75F, .min = 0.0F, .max = 1.0F, .step = 0.05F});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUICheckbox")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 652.0F, .w = 200.0F, .h = 24.0F}})
		.set<engine::ui::Checkbox>({.checked = true, .label = "Mute", .font_size = 18.0F, .box_size = 20.0F});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUIRadio")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 684.0F, .w = 200.0F, .h = 24.0F}})
		.set<engine::ui::Radio>({
			.group = "difficulty",
			.value = "hard",
			.selected = true,
			.label = "Hard",
			.font_size = 18.0F,
			.radius = 12.0F,
		});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUITextInput")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 716.0F, .w = 200.0F, .h = 28.0F}})
		.set<engine::ui::TextInput>({
			.text = "Hello",
			.placeholder = "Name",
			.max_length = 32,
			.font_size = 18.0F,
		});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUITextArea")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 752.0F, .w = 200.0F, .h = 80.0F}})
		.set<engine::ui::TextArea>({
			.text = "Line1",
			.placeholder = "Notes",
			.max_length = 128,
			.font_size = 18.0F,
			.line_spacing = 5.0F,
		});
	test_world.World()
		.entity()
		.child_of(scene_root)
		.set_name("TestUIDropdown")
		.set<engine::ui::UIRect>({{.x = 10.0F, .y = 840.0F, .w = 200.0F, .h = 28.0F}})
		.set<engine::ui::Dropdown>({.placeholder = "Choose", .font_size = 18.0F, .row_height = 30.0F});

	return SerializeSceneRoot(test_world.World(), scene_root);
}

json BuildJsonWorld() {
	TestWorld test_world;
	const flecs::entity scene_root = test_world.World().entity("SceneRoot");

	const int count = engine::level::LoadLevelFromSource(test_world.World(), kTestSceneJson, scene_root);
	if (count < 0) {
		throw std::runtime_error("LoadLevelFromSource failed for test scene");
	}

	return SerializeSceneRoot(test_world.World(), scene_root);
}

json BuildScriptWorld() {
	TestWorld test_world;
	const flecs::entity scene_root = test_world.World().entity("SceneRoot");

	engine::scripting::RegisterGlobalFunctionForScripts(
		test_world.World(),
		engine::scripting::ScriptMethodSignature{
			.name = "CreateNamedEntity",
			.return_type = engine::scripting::ScriptValueType::MakeObject(
				test_world.World().component<engine::scripting::ScriptEntityRef>().id()
			),
			.params = {{
				.type = engine::scripting::ScriptValueType::MakeString(),
				.by_reference = true,
				.name = "name",
			}},
		},
		[](engine::scripting::ScriptCallContext& ctx, const flecs::world& world) {
			const flecs::entity scene_root = world.lookup("SceneRoot");
			const flecs::entity entity = world.entity();
			if (scene_root) {
				entity.child_of(scene_root);
			}
			entity.set_name(ctx.GetArgString(0).c_str());
			new (ctx.GetReturnObjectLocation()) engine::scripting::ScriptEntityRef(entity);
		}
	);

	engine::scripting::RegisterGlobalFunctionForScripts(
		test_world.World(),
		engine::scripting::ScriptMethodSignature{
			.name = "MakeRgba",
			.return_type =
				engine::scripting::ScriptValueType::MakeObject(test_world.World().component<engine::core::Rgba>().id()),
			.params =
				{
					{.type = engine::scripting::ScriptValueType::MakeInt(), .name = "r"},
					{.type = engine::scripting::ScriptValueType::MakeInt(), .name = "g"},
					{.type = engine::scripting::ScriptValueType::MakeInt(), .name = "b"},
					{.type = engine::scripting::ScriptValueType::MakeInt(), .name = "a"},
				},
		},
		[](engine::scripting::ScriptCallContext& ctx, flecs::world&) {
			new (ctx.GetReturnObjectLocation()) engine::core::Rgba{
				.r = static_cast<std::uint8_t>(ctx.GetArgInt(0)),
				.g = static_cast<std::uint8_t>(ctx.GetArgInt(1)),
				.b = static_cast<std::uint8_t>(ctx.GetArgInt(2)),
				.a = static_cast<std::uint8_t>(ctx.GetArgInt(3)),
			};
		}
	);

	const flecs::entity script_entity = test_world.World().entity("SceneInitScript").child_of(scene_root);
	script_entity.set<engine::scripting::ScriptComponent>({
		.inline_source = std::string{kTestSceneScript},
	});

	if (auto* script_component = script_entity.try_get_mut<engine::scripting::ScriptComponent>()) {
		if (script_component->error != engine::scripting::ScriptError::None) {
			throw std::runtime_error(
				script_component->error_message.empty() ? "Inline scene script failed" : script_component->error_message
			);
		}
		if (!script_component->instance) {
			throw std::runtime_error("Inline scene script instance was not created");
		}
		script_component->instance->OnInit(scene_root);
		script_component->initialized = true;
	}
	else {
		throw std::runtime_error("SceneInitScript missing ScriptComponent after setup");
	}

	return SerializeSceneRoot(test_world.World(), scene_root);
}

} // namespace

int main(const int argc, char** argv) {
	try {
		spdlog::set_level(spdlog::level::trace);

		// First argument is the output directory; defaults to "test_output" in the working directory.
		const std::filesystem::path output_dir = (argc >= 2) ? argv[1] : "test_output";
		std::filesystem::create_directories(output_dir);

		const json json_world = BuildJsonWorld();
		WriteJsonFile(output_dir / "world_json.json", json_world);
		const json as_world = BuildScriptWorld();
		WriteJsonFile(output_dir / "world_as.json", as_world);
		const json cpp_world = BuildCppWorld();
		WriteJsonFile(output_dir / "world_cpp.json", cpp_world);

		const std::vector<DiffRow> rows = ComputeDiff(json_world, as_world, cpp_world);
		WriteDiffTable(output_dir / "diff_table.txt", rows);

		size_t mismatches = 0;
		for (const auto& row : rows) {
			if (row.mismatch) {
				++mismatches;
			}
		}

		std::cout
			<< "Scene equivalence diff complete: "
			<< rows.size()
			<< " properties, "
			<< mismatches
			<< " mismatches\n";
		return mismatches == 0 ? 0 : 1;
	}
	catch (const std::exception& e) {
		std::cerr << "scene_equivalence_test failed: " << e.what() << '\n';
		return 1;
	}
}
