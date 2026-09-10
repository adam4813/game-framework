#include "render_module.hpp"

#include <spdlog/spdlog.h>

#include <flecs.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "engine/assets/assets.hpp"
#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"
#include "engine/platform/platform.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"
#include "engine/spatial/spatial.hpp"
#include "render_components.hpp"

namespace engine::render {

namespace {
using json = nlohmann::json;

// Tracks whether BeginMode3D was actually issued this frame so the primitive systems only draw
// while a 3D pass is open and EndMode3D is balanced against BeginMode3D even when no camera
// exists.
struct Render3DState {
	bool active{false};
};

// Builds a DrawMaterial from a material entity already resolved via e.target<RenderWith>().
// AlbedoMap and ShaderMap are components on the material entity itself (materials own their
// texture/shader references, consistent with how AAA engines structure material assets).
platform::DrawMaterial MakeDrawMaterial(const flecs::entity mat_e) {
	const auto* mat = mat_e.try_get<Material>();
	if (!mat) return {};
	const auto* albedo = mat_e.try_get<AlbedoMap>();
	const auto* shader = mat_e.try_get<ShaderMap>();
	return {
		.texture = albedo ? albedo->handle : -1,
		.shader = shader ? shader->handle : -1,
		.tint = mat->color,
		.wireframe = mat->wireframe,
		.cast_shadow = mat->cast_shadow,
	};
}

// Registers a draw system for primitive component TPrim. The per-primitive draw calls differ, so it is
// passed as a callable: draw(platform, worldMatrix, primitive, material).
template<typename TPrim, typename Draw>
void RegisterPrimitiveRenderer(const flecs::world& world, const char* name, Draw draw) {
	world.system<const spatial::WorldTransform, const TPrim>(name)
		.kind(flecs::OnStore)
		.template with<RenderWith>(flecs::Wildcard)
		.self()
		.up()
		.each([draw](const flecs::iter& it, const size_t i, const spatial::WorldTransform& wt, const TPrim& prim) {
			if (!it.world().get<Render3DState>().active) return;
			const auto platform_ref = it.world().get<platform::PlatformRef>();
			draw(*platform_ref.ptr, wt.matrix, prim, MakeDrawMaterial(it.entity(i).target<RenderWith>()));
		})
		.template add<scene::GameScene>();
}

void RegisterRenderLevelLoaders(const flecs::world& world) {
	using core::JRgba;
	using core::JVec2;
	using core::JVec3;

	level::RegisterComponentLoader(world, "camera", [](const flecs::entity e, const json& j) {
		Camera c{};
		c.up = JVec3(j, "up", c.up);
		c.fov = j.value("fov", c.fov);
		c.aspect_ratio = j.value("aspect_ratio", c.aspect_ratio);
		c.near_plane = j.value("near", c.near_plane);
		c.far_plane = j.value("far", c.far_plane);
		e.set<Camera>(c);
	});
	level::RegisterComponentLoader(world, "cube", [](const flecs::entity e, const json& j) {
		e.set<CubePrimitive>({JVec3(j, "size", glm::vec3{1.0F})});
	});
	level::RegisterComponentLoader(world, "sphere", [](const flecs::entity e, const json& j) {
		e.set<SpherePrimitive>({j.value("radius", 0.5F)});
	});
	level::RegisterComponentLoader(world, "quad", [](const flecs::entity e, const json& j) {
		e.set<QuadPrimitive>({JVec2(j, "size", glm::vec2{1.0F})});
	});
	level::RegisterComponentLoader(world, "capsule", [](const flecs::entity e, const json& j) {
		e.set<CapsulePrimitive>({j.value("radius", 0.5F), j.value("height", 1.0F)});
	});
	level::RegisterComponentLoader(world, "mesh", [](const flecs::entity e, const json& j) {
		e.set<MeshPrimitive>({.path = assets::ResolveAsset(e.world(), j.value("path", std::string{}))});
	});
	level::RegisterComponentLoader(world, "material", [](const flecs::entity e, const json& j) {
		Material m{};
		m.color = JRgba(j, "color", m.color);
		m.wireframe = j.value("wireframe", m.wireframe);
		m.cast_shadow = j.value("cast_shadow", m.cast_shadow);
		e.set<Material>(m);
	});
	level::RegisterComponentLoader(world, "albedo", [](const flecs::entity e, const json& j) {
		e.set<AlbedoMap>({.path = assets::ResolveAsset(e.world(), j.value("path", std::string{}))});
	});
	level::RegisterComponentLoader(world, "shader", [](const flecs::entity e, const json& j) {
		ShaderMap sm{};
		if (j.contains("stages")) {
			for (const auto& s : j.at("stages")) {
				sm.stages.push_back({.type = s.value("type", std::string{}), .path = s.value("path", std::string{})});
			}
		}
		e.set<ShaderMap>(sm);
	});
	level::RegisterComponentLoader(world, "directional_light", [](const flecs::entity e, const json& j) {
		DirectionalLight dl{};
		dl.direction = JVec3(j, "direction", dl.direction);
		dl.color = JRgba(j, "color", dl.color);
		dl.intensity = j.value("intensity", dl.intensity);
		dl.specular_strength = j.value("specular_strength", dl.specular_strength);
		dl.shininess = j.value("shininess", dl.shininess);
		dl.casts_shadows = j.value("casts_shadows", dl.casts_shadows);
		dl.shadow_ground_y = j.value("shadow_ground_y", dl.shadow_ground_y);
		dl.shadow_color = JRgba(j, "shadow_color", dl.shadow_color);
		e.set<DirectionalLight>(dl);
	});

	level::RegisterSingletonLoader(world, "ambient_light", [](const flecs::world& w, const json& j) {
		AmbientLight al{};
		al.color = JRgba(j, "color", al.color);
		al.intensity = j.value("intensity", al.intensity);
		w.set<AmbientLight>(al);
	});

	// render_with: the parent renderable uses the freshly built (nested) child as its material entity.
	level::RegisterLink(world, "render_with", [](const flecs::entity parent, const flecs::entity child) {
		parent.add<RenderWith>(child);
	});

	// look_at: aim a camera at another named entity — the 3D pass reads that entity's WorldTransform.
	level::RegisterRef(world, "look_at", [](const flecs::entity self, const flecs::entity target) {
		self.add<LookAt>(target);
	});
}
} // namespace

RenderModule::RenderModule(const flecs::world& world) {
	world.set<Render3DState>({});

	// === Reflection ===
	world.component<RenderWith>().add(flecs::Exclusive);
	world.component<LookAt>().add(flecs::Exclusive);
	world.component<Camera>()
		.member<glm::vec3>("up")
		.member<float>("fov")
		.member<float>("aspect_ratio")
		.member<float>("near_plane")
		.member<float>("far_plane");
	world.component<CubePrimitive>().member<glm::vec3>("size");
	world.component<SpherePrimitive>().member<float>("radius");
	world.component<QuadPrimitive>().member<glm::vec2>("size");
	world.component<CapsulePrimitive>().member<float>("radius").member<float>("height");
	world.component<MeshPrimitive>().member<std::string>("path").member<int>("handle");
	world.component<DynamicMesh>().member<int>("handle");
	world.component<AmbientLight>().member<core::Rgba>("color").member<float>("intensity");
	world.component<DirectionalLight>()
		.member<glm::vec3>("direction")
		.member<core::Rgba>("color")
		.member<float>("intensity")
		.member<float>("specular_strength")
		.member<float>("shininess")
		.member<bool>("casts_shadows")
		.member<float>("shadow_ground_y")
		.member<core::Rgba>("shadow_color");
	world.component<Material>()
		.member<core::Rgba>("color")
		.member<bool>("wireframe")
		.member<bool>("cast_shadow")
		.add<scripting::ScriptTraversal>(world.component<RenderWith>());
	world.component<AlbedoMap>().member<std::string>("path").member<int>("handle").add<scripting::ScriptTraversal>(
		world.component<RenderWith>()
	);
	world.component<ShaderMap>().member<int>("handle").add<scripting::ScriptTraversal>(world.component<RenderWith>());

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<Camera>());
	scripting::RegisterComponentForScripts(world, world.component<CubePrimitive>());
	scripting::RegisterComponentForScripts(world, world.component<SpherePrimitive>());
	scripting::RegisterComponentForScripts(world, world.component<QuadPrimitive>());
	scripting::RegisterComponentForScripts(world, world.component<CapsulePrimitive>());
	scripting::RegisterComponentForScripts(world, world.component<MeshPrimitive>());
	scripting::RegisterComponentForScripts(world, world.component<AmbientLight>());
	scripting::RegisterComponentForScripts(world, world.component<DirectionalLight>());
	scripting::RegisterComponentForScripts(world, world.component<Material>());
	scripting::RegisterComponentForScripts(world, world.component<AlbedoMap>());

	// === Level loader ===
	RegisterRenderLevelLoaders(world);

	// === Path-asset resolvers ===
	assets::RegisterPathAsset<AlbedoMap>(world, assets::AssetType::Texture);
	assets::RegisterPathAsset<MeshPrimitive>(world, assets::AssetType::Mesh);

	std::ignore =
		world.observer<DynamicMesh>("DeleteDynamicMesh")
			.event(flecs::OnRemove)
			.each([](const flecs::iter& it, size_t, const DynamicMesh& mesh) {
				if (mesh.handle < 0) {
					spdlog::warn(
						"[RenderModule] DynamicMesh handle is negative; ensure it was uploaded via UploadDynamicMesh"
					);
				}
				const auto& platform_ref = it.world().get<platform::PlatformRef>();
				platform_ref.ptr->UnloadDynamicMesh(mesh.handle);
			});

	// === OBSERVER: Load shader from stages when ShaderMap is set ===
	std::ignore = world.observer<ShaderMap>("ResolveShaderMap")
					  .event(flecs::OnSet)
					  .each([](const flecs::iter& it, size_t, ShaderMap& sm) {
						  if (sm.stages.empty()) return;
						  const auto& platform_ref = it.world().get<platform::PlatformRef>();
						  const std::string dir = platform_ref.ptr->ShaderDirectory();
						  std::vector<platform::ShaderStage> resolved;
						  resolved.reserve(sm.stages.size());
						  for (const auto& stage : sm.stages) {
							  resolved.push_back({.type = stage.type, .path = dir + "/" + stage.path});
						  }
						  sm.handle = platform_ref.ptr->LoadShader(resolved);
					  });

	// === OBSERVER: Release shader on removal ===
	std::ignore = world.observer<ShaderMap>("ReleaseShaderMap")
					  .event(flecs::OnRemove)
					  .each([](const flecs::iter& it, size_t, const ShaderMap& sm) {
						  if (sm.handle >= 0) {
							  it.world().get<platform::PlatformRef>().ptr->UnloadShader(sm.handle);
						  }
					  });

	// Camera query: the camera's own WorldTransform (eye, field 0) + Camera (field 1), then the LookAt
	// term (field 2) binding the aim entity into the $target variable, then that target's optional
	// WorldTransform read from $target (field 3). The binding term precedes the read so $target is
	// bound before it is dereferenced; both are optional so a camera without a LookAt still matches
	// (field 3 unset → fall back to the camera's own forward). The aim point is thus derived directly
	// from the target entity's transform; the camera stores no target position.
	const auto camera_query = world.query_builder<const spatial::WorldTransform, const Camera>()
								  .with<LookAt>()
								  .second("$target")
								  .optional()
								  .with<const spatial::WorldTransform>()
								  .src("$target")
								  .optional()
								  .build();

	// === SYSTEM: Open the 3D pass ===
	// Sets the view and projection matrices for subsequent render calls
	std::ignore =
		world.system("Render3DBegin")
			.kind(flecs::OnStore)
			.run([camera_query](const flecs::iter& it) {
				const auto& platform_ref = it.world().get<platform::PlatformRef>();
				auto& state = it.world().get_mut<Render3DState>();
				state.active = false;
				if (!camera_query.is_true()) {
					spdlog::warn("[Render3DBegin] No camera found; skipping 3D pass");
					return;
				}

				const auto camera = camera_query.first();
				const auto eye = camera.get<const spatial::WorldTransform>();
				const auto cam = camera.get<const Camera>();
				const flecs::entity target_ent = camera.target<const LookAt>();
				const auto* target = target_ent ? target_ent.try_get<const spatial::WorldTransform>() : nullptr;

				const glm::vec3 aim = target
										  ? target->position
										  : eye.position + glm::vec3(eye.matrix * glm::vec4(0.0F, 0.0F, -1.0F, 0.0F));
				platform_ref.ptr->BeginMode3D({eye.position, aim, cam.up, cam.fov});
				state.active = true;

				// When multi-camera is supported, use this query and add render targets
				/*
				bool opened = false;
				camera_query.run([&](flecs::iter& camera_it) {
					while (camera_it.next()) {
						if (opened) break;
						const auto eye = camera_it.field<const spatial::WorldTransform>(0);
						const auto cam = camera_it.field<const Camera>(1);
						// field 3 is the optional LookAt target's WorldTransform
						const bool has_target = camera_it.is_set(3);
						for (const auto row : camera_it) {
							if (opened) break;
							// Aim at the LookAt target, or along the camera's own forward when it has none.
							const glm::vec3 aim =
								has_target ? camera_it.field<const spatial::WorldTransform>(3)[row].position
										   : eye[row].position
												 + glm::vec3(eye[row].matrix * glm::vec4(0.0F, 0.0F, -1.0F, 0.0F));
							platform_ref.ptr->BeginMode3D({eye[row].position, aim, cam[row].up, cam[row].fov});
							state.active = true;
							opened = true;
						}
					}
				});
				*/
			})
			.add<scene::GameScene>();

	const auto light_query = world.query_builder<const DirectionalLight>().build();

	// === SYSTEM: Upload scene lighting ===
	std::ignore = world.system("RenderLightingUpload")
					  .kind(flecs::OnStore)
					  .run([light_query](const flecs::iter& it) {
						  const auto& platform_ref = it.world().get<platform::PlatformRef>();
						  if (!it.world().get<Render3DState>().active) return;
						  platform::LightParams params;
						  if (const auto light = light_query.first()) {
							  const auto& dl = light.get<const DirectionalLight>();
							  params.direction = dl.direction;
							  params.light_color = dl.color;
							  params.light_intensity = dl.intensity;
							  params.specular_strength = dl.specular_strength;
							  params.shininess = dl.shininess;
							  params.shadows_enabled = dl.casts_shadows;
							  params.shadow_ground_y = dl.shadow_ground_y;
							  params.shadow_color = dl.shadow_color;
						  }
						  if (it.world().has<AmbientLight>()) {
							  const auto& al = it.world().get<AmbientLight>();
							  params.ambient_color = al.color;
							  params.ambient_intensity = al.intensity;
						  }
						  platform_ref.ptr->SetLighting(params);
					  })
					  .add<scene::GameScene>();

	// === SYSTEMS: Draw primitives ===
	RegisterPrimitiveRenderer<CubePrimitive>(
		world,
		"RenderCubes",
		[](platform::Platform& p, const glm::mat4& m, const CubePrimitive& cube, const platform::DrawMaterial& mat) {
			p.DrawCube(m, cube.size, mat);
		}
	);
	RegisterPrimitiveRenderer<SpherePrimitive>(
		world,
		"RenderSpheres",
		[](platform::Platform& p,
		   const glm::mat4& m,
		   const SpherePrimitive& sphere,
		   const platform::DrawMaterial& mat) { p.DrawSphere(m, sphere.radius, mat); }
	);
	RegisterPrimitiveRenderer<QuadPrimitive>(
		world,
		"RenderQuads",
		[](platform::Platform& p, const glm::mat4& m, const QuadPrimitive& quad, const platform::DrawMaterial& mat) {
			p.DrawQuad(m, quad.size, mat);
		}
	);
	RegisterPrimitiveRenderer<CapsulePrimitive>(
		world,
		"RenderCapsules",
		[](platform::Platform& p, const glm::mat4& m, const CapsulePrimitive& cap, const platform::DrawMaterial& mat) {
			p.DrawCapsule(m, cap.radius, cap.height, mat);
		}
	);
	RegisterPrimitiveRenderer<MeshPrimitive>(
		world,
		"RenderMeshes",
		[](platform::Platform& p, const glm::mat4& m, const MeshPrimitive& mesh, const platform::DrawMaterial& mat) {
			if (mesh.handle < 0) return;
			p.DrawMesh(mesh.handle, m, mat);
		}
	);
	RegisterPrimitiveRenderer<DynamicMesh>(
		world,
		"RenderDynamicMeshes",
		[](platform::Platform& p, const glm::mat4& m, const DynamicMesh& mesh, const platform::DrawMaterial& mat) {
			if (mesh.handle < 0) return;
			p.DrawDynamicMesh(mesh.handle, m, mat);
		}
	);

	// === SYSTEM: Close the 3D pass ===
	std::ignore = world.system("Render3DEnd")
					  .kind(flecs::OnStore)
					  .run([](const flecs::iter& it) {
						  auto& state = it.world().get_mut<Render3DState>();
						  if (state.active) {
							  const auto& platform_ref = it.world().get<platform::PlatformRef>();
							  platform_ref.ptr->EndMode3D();
						  }
						  state.active = false;
					  })
					  .add<scene::GameScene>();

	spdlog::info("[RenderModule] Registered render systems with Flecs");
}

} // namespace engine::render
