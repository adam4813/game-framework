#include "render_module.hpp"

#include <spdlog/spdlog.h>

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/assets/assets.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/platform/platform.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"
#include "render_components.hpp"

namespace engine::render {

namespace {
// Tracks whether BeginMode3D was actually issued this frame so the primitive systems only draw
// while a 3D pass is open and EndMode3D is balanced against BeginMode3D even when no camera
// exists.
struct Render3DState {
	bool active{false};
};

// Registers an OnSet observer for any TextureMapBase-derived component T. When T is set on
// an entity with a non-empty path, the platform handle is resolved into T.handle.
template<typename T>
void RegisterTextureMapObserver(const flecs::world& world) {
	world.observer<T>((std::string{"Resolve"} + std::string{world.component<T>().name()}).c_str())
		.event(flecs::OnSet)
		.each([](const flecs::iter& it, size_t, T& map) { map.handle = assets::LoadTexture(it.world(), map.path); });
}
} // namespace

RenderModule::RenderModule(const flecs::world& world) {
	world.set<Render3DState>({});

	// === Reflection ===
	world.component<CubePrimitive>().member<glm::vec3>("size");
	world.component<SpherePrimitive>().member<float>("radius");
	world.component<QuadPrimitive>().member<glm::vec2>("size");
	world.component<CapsulePrimitive>().member<float>("radius").member<float>("height");
	world.component<MeshPrimitive>().member<std::string>("path").member<int>("handle");
	world.component<DynamicMesh>().member<int>("handle");
	world.component<Material>().member<platform::Rgba>("color").member<bool>("wireframe").member<bool>("cast_shadow");
	world.component<AlbedoMap>().member<std::string>("path").member<int>("handle");
	world.component<AmbientLight>().member<platform::Rgba>("color").member<float>("intensity");
	world.component<DirectionalLight>()
		.member<glm::vec3>("direction")
		.member<platform::Rgba>("color")
		.member<float>("intensity")
		.member<float>("specular_strength")
		.member<float>("shininess")
		.member<bool>("casts_shadows")
		.member<float>("shadow_ground_y")
		.member<platform::Rgba>("shadow_color");

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<CubePrimitive>());
	scripting::RegisterComponentForScripts(world, world.component<SpherePrimitive>());
	scripting::RegisterComponentForScripts(world, world.component<QuadPrimitive>());
	scripting::RegisterComponentForScripts(world, world.component<CapsulePrimitive>());
	scripting::RegisterComponentForScripts(world, world.component<AmbientLight>());
	scripting::RegisterComponentForScripts(world, world.component<DirectionalLight>());
	scripting::RegisterComponentForScripts(world, world.component<Material>());
	scripting::RegisterComponentForScripts(world, world.component<AlbedoMap>());
	scripting::RegisterComponentForScripts(world, world.component<MeshPrimitive>());

	// === OBSERVER: Recompute camera matrices when its world transform changes ===
	world.observer<ecs::WorldTransform, Camera>("CameraTransformUpdate")
		.event(flecs::OnSet)
		.each([](const ecs::WorldTransform& world_transform, Camera& camera) {
			camera.view_matrix = glm::lookAt(world_transform.position, camera.target, camera.up);
			camera.projection_matrix =
				glm::perspective(glm::radians(camera.fov), camera.aspect_ratio, camera.near_plane, camera.far_plane);
		});

	// === Texture resolver template ===
	RegisterTextureMapObserver<AlbedoMap>(world);

	// === OBSERVER: Release AlbedoMap texture on removal ===
	world.observer<AlbedoMap>("ReleaseAlbedoMap")
		.event(flecs::OnRemove)
		.each([](const flecs::iter& it, size_t, AlbedoMap& map) {
			if (!map.path.empty()) {
				assets::Release(it.world(), assets::AssetType::Texture, map.path);
			}
		});

	// === OBSERVER: Resolve a MeshPrimitive's handle from its path (mirrors the texture resolvers,
	// using the mesh loader). Keeps handle resolution event-driven instead of lazily loading inside
	// the per-frame RenderMeshes draw system. ===
	world.observer<MeshPrimitive>("ResolveMeshPrimitive")
		.event(flecs::OnSet)
		.each([](const flecs::iter& it, size_t, MeshPrimitive& mesh) {
			mesh.handle = assets::LoadMesh(it.world(), mesh.path);
		});

	// === OBSERVER: Release MeshPrimitive mesh on removal ===
	world.observer<MeshPrimitive>("ReleaseMeshPrimitive")
		.event(flecs::OnRemove)
		.each([](const flecs::iter& it, size_t, MeshPrimitive& mesh) {
			if (!mesh.path.empty()) {
				assets::Release(it.world(), assets::AssetType::Mesh, mesh.path);
			}
		});

	world.observer<DynamicMesh>("DeleteDynamicMesh")
		.event(flecs::OnRemove)
		.each([](const flecs::iter& it, size_t, DynamicMesh& mesh) {
			if (mesh.handle < 0) {
				spdlog::warn(
					"[RenderModule] DynamicMesh handle is negative; ensure it was uploaded via UploadDynamicMesh"
				);
			}
			const auto& platform_ref = it.world().get<platform::PlatformRef>();
			platform_ref.ptr->UnloadDynamicMesh(mesh.handle);
		});

	const auto camera_query = world.query_builder<const ecs::WorldTransform, const Camera>().build();
	const auto light_query = world.query_builder<const DirectionalLight>().build();

	// === SYSTEM: Open the 3D pass ===
	// Reads the camera's WorldTransform.position so cameras can be parented
	// (e.g. a follow-camera as a child of the player, positioned by the transform propagation system).
	world.system("Render3DBegin")
		.kind(flecs::OnStore)
		.run([camera_query](const flecs::iter& it) {
			const auto& platform_ref = it.world().get<platform::PlatformRef>();
			auto& state = it.world().get_mut<Render3DState>();
			state.active = false;
			const auto camera = camera_query.first();
			if (!camera) return;
			const auto& world_transform = camera.get<const ecs::WorldTransform>();
			const auto& cam = camera.get<const Camera>();
			platform_ref.ptr->BeginMode3D({world_transform.position, cam.target, cam.up, cam.fov});
			state.active = true;
		})
		.add<scene::GameScene>();

	// === SYSTEM: Upload scene lighting ===
	world.system("RenderLightingUpload")
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

	// Helper: get the albedo texture handle for the current entity (0-indexed term i in each).
	// Returns -1 when the entity has no AlbedoMap or the handle is unresolved.
	auto albedo_handle = [](const flecs::iter& it, const size_t i) -> int {
		if (const auto* a = it.entity(i).try_get<AlbedoMap>()) {
			return a->handle;
		}
		return -1;
	};

	// === SYSTEM: Draw cubes ===
	world.system<const ecs::WorldTransform, const CubePrimitive, const Material>("RenderCubes")
		.kind(flecs::OnStore)
		.each([albedo_handle](
				  const flecs::iter& it,
				  const size_t i,
				  const ecs::WorldTransform& wt,
				  const CubePrimitive& cube,
				  const Material& mat
			  ) {
			const auto& platform_ref = it.world().get<platform::PlatformRef>();
			if (!it.world().get<Render3DState>().active) return;
			platform_ref.ptr
				->DrawCube(wt.matrix, cube.size, mat.color, albedo_handle(it, i), mat.cast_shadow, mat.wireframe);
		})
		.add<scene::GameScene>();

	// === SYSTEM: Draw spheres ===
	world.system<const ecs::WorldTransform, const SpherePrimitive, const Material>("RenderSpheres")
		.kind(flecs::OnStore)
		.each([albedo_handle](
				  const flecs::iter& it,
				  const size_t i,
				  const ecs::WorldTransform& wt,
				  const SpherePrimitive& sphere,
				  const Material& mat
			  ) {
			const auto& platform_ref = it.world().get<platform::PlatformRef>();
			if (!it.world().get<Render3DState>().active) return;
			platform_ref.ptr
				->DrawSphere(wt.matrix, sphere.radius, mat.color, albedo_handle(it, i), mat.cast_shadow, mat.wireframe);
		})
		.add<scene::GameScene>();

	// === SYSTEM: Draw quads ===
	world.system<const ecs::WorldTransform, const QuadPrimitive, const Material>("RenderQuads")
		.kind(flecs::OnStore)
		.each([albedo_handle](
				  const flecs::iter& it,
				  const size_t i,
				  const ecs::WorldTransform& wt,
				  const QuadPrimitive& quad,
				  const Material& mat
			  ) {
			if (!it.world().get<Render3DState>().active) return;
			const auto& platform_ref = it.world().get<platform::PlatformRef>();
			platform_ref.ptr
				->DrawQuad(wt.matrix, quad.size, mat.color, albedo_handle(it, i), mat.cast_shadow, mat.wireframe);
		})
		.add<scene::GameScene>();

	// === SYSTEM: Draw capsules ===
	world.system<const ecs::WorldTransform, const CapsulePrimitive, const Material>("RenderCapsules")
		.kind(flecs::OnStore)
		.each([albedo_handle](
				  const flecs::iter& it,
				  const size_t i,
				  const ecs::WorldTransform& wt,
				  const CapsulePrimitive& capsule,
				  const Material& mat
			  ) {
			if (!it.world().get<Render3DState>().active) return;
			const auto& platform_ref = it.world().get<platform::PlatformRef>();
			platform_ref.ptr->DrawCapsule(
				wt.matrix,
				capsule.radius,
				capsule.height,
				mat.color,
				albedo_handle(it, i),
				mat.cast_shadow,
				mat.wireframe
			);
		})
		.add<scene::GameScene>();

	// === SYSTEM: Draw meshes ===
	world.system<const ecs::WorldTransform, const MeshPrimitive, const Material>("RenderMeshes")
		.kind(flecs::OnStore)
		.each([albedo_handle](
				  const flecs::iter& it,
				  const size_t i,
				  const ecs::WorldTransform& wt,
				  const MeshPrimitive& mesh,
				  const Material& mat
			  ) {
			if (!it.world().get<Render3DState>().active) return;
			if (mesh.handle < 0) return;
			const auto& platform_ref = it.world().get<platform::PlatformRef>();
			platform_ref.ptr
				->DrawMesh(mesh.handle, wt.matrix, mat.color, albedo_handle(it, i), mat.cast_shadow, mat.wireframe);
		})
		.add<scene::GameScene>();

	// === SYSTEM: Draw dynamic meshes ===
	world.system<const ecs::WorldTransform, const DynamicMesh, const Material>("RenderDynamicMeshes")
		.kind(flecs::OnStore)
		.each([albedo_handle](
				  const flecs::iter& it,
				  const size_t i,
				  const ecs::WorldTransform& wt,
				  const DynamicMesh& mesh,
				  const Material& mat
			  ) {
			if (!it.world().get<Render3DState>().active) return;
			if (mesh.handle < 0) return;
			const auto& platform_ref = it.world().get<platform::PlatformRef>();
			platform_ref.ptr->DrawDynamicMesh(mesh.handle, wt.matrix, albedo_handle(it, i), mat.wireframe);
		})
		.add<scene::GameScene>();

	// === SYSTEM: Close the 3D pass ===
	world.system("Render3DEnd")
		.kind(flecs::OnStore)
		.run([](const flecs::iter& it) {
			const auto& platform_ref = it.world().get<platform::PlatformRef>();
			auto& state = it.world().get_mut<Render3DState>();
			if (state.active) {
				platform_ref.ptr->EndMode3D();
			}
			state.active = false;
		})
		.add<scene::GameScene>();

	spdlog::info("[RenderModule] Registered render systems with Flecs");
}

} // namespace engine::render
