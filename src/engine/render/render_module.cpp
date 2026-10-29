#include "render_module.hpp"

#include <atomic>
#include <chrono>
#include <mutex>

#include <spdlog/spdlog.h>

#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

#include "camera_projection.hpp"
#include "engine/assets/assets.hpp"
#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"
#include "engine/platform/platform.hpp"
#include "engine/profiling/profiling.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"
#include "engine/spatial/spatial.hpp"
#include "render_components.hpp"
#include "render_queue.hpp"

namespace engine::render {

namespace {
using json = nlohmann::json;

// Tracks whether BeginMode3D was actually issued this frame so the primitive systems only draw
// while a 3D pass is open and EndMode3D is balanced against BeginMode3D even when no camera
// exists.
struct Render3DState {
	bool active{false};
};

// Timestamp captured when the 3D pass opens, used to measure how long the primitive enqueue systems
// take (from pass open until the flush). Single world / single-threaded pipeline, so a file static
// is sufficient for this diagnostic.
std::chrono::steady_clock::time_point g_render_begin_time;

// Guards RenderQueue::commands during multi-threaded primitive enqueue. Each system run accumulates
// into a local vector (zero contention) and merges here once per invocation.
std::mutex g_render_queue_mutex;

// Thread-counting for the primitive enqueue systems. Uses the same generation-stamp idiom as the
// pathfinding module: Render3DBegin bumps g_render_frame_gen; each enqueuer run callback registers
// itself once per generation via thread_local storage. Plotted as render.enqueue_threads.
std::atomic<int64_t> g_render_threads_active{0};
std::atomic<int64_t> g_render_frame_gen{0};
thread_local int64_t tl_render_frame_gen = -1;

// Builds a DrawMaterial from a material entity already resolved via e.target<RenderWith>().
// AlbedoMap and ShaderMap are components on the material entity itself (materials own their
// texture/shader references, consistent with how AAA engines structure material assets).
platform::DrawMaterial MakeDrawMaterial(const flecs::entity mat_e) {
	// Guard an invalid (0) material entity — e.g. a RenderWith target that is unset or not yet
	// resolved. is_valid() is safe for id 0, whereas try_get/ecs_get_id on 0 asserts (debug) or reads
	// out of bounds (release) under flecs 4.x. A material-less primitive renders with the default.
	if (!mat_e.is_valid()) return {};
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
		.unlit = mat->unlit,
	};
}

// Resolves an entity's draw material. Inline components on the entity itself take precedence over a
// RenderWith material entity. The inline path is important for performance: high-count entities
// (e.g. thousands of agents) that each need a distinct colour should carry a plain Material value
// rather than a unique RenderWith target — a unique relationship pair per entity would put every
// entity in its own Flecs table (archetype fragmentation) and destroy iteration performance.
// Falls back to the parent entity's RenderWith for overflow chunk children that inherit material.
platform::DrawMaterial ResolveDrawMaterial(const flecs::entity e) {
	if (e.has<Material>()) return MakeDrawMaterial(e);
	const flecs::entity mat = e.target<RenderWith>();
	if (mat.is_valid()) return MakeDrawMaterial(mat);
	// Overflow chunk entities are children of a carrier that owns the RenderWith relationship.
	const flecs::entity parent = e.parent();
	return MakeDrawMaterial(parent.is_valid() ? parent.target<RenderWith>() : flecs::entity{});
}

// Registers a draw system for primitive component TPrim. The per-primitive draw calls differ, so it is
// passed as a callable: draw(platform, worldMatrix, primitive, material). RenderWith is optional so
// entities carrying an inline Material (no relationship) are still drawn.
template<typename TPrim, typename Draw>
void RegisterPrimitiveRenderer(const flecs::world& world, const char* name, Draw draw) {
	world.system<const spatial::WorldTransform, const TPrim>(name)
		.kind(flecs::OnStore)
		.template with<RenderWith>(flecs::Wildcard)
		.optional()
		.self()
		.up()
		.each([draw](const flecs::iter& it, const size_t i, const spatial::WorldTransform& wt, const TPrim& prim) {
			if (!it.world().get<Render3DState>().active) return;
			const auto platform_ref = it.world().get<platform::PlatformRef>();
			const auto entity = it.entity(i);
			// Entities must have a material source: inline Material, a direct (RenderWith,*) pair,
			// or a parent with (RenderWith,*) — the latter covers overflow chunk children whose
			// atlas/material is inherited from the carrier entity.
			if (!entity.has<Material>()
				&& !entity.has<RenderWith>(flecs::Wildcard)
				&& !entity.parent().has<RenderWith>(flecs::Wildcard)) {
				return;
			}
			// Opt-in sphere frustum cull: only applied to entities that carry FrustumCullRadius
			// (e.g. per-entity creature/NPC sprites). Static or streaming meshes omit this
			// component and are never position-culled.
			if (const auto* fcr = entity.try_get<FrustumCullRadius>()) {
				const auto& cull = it.world().get<CameraFrustum>();
				if (cull.valid && !cull.frustum.ContainsSphere(glm::vec3(wt.matrix[3]), fcr->radius)) return;
			}
			draw(*platform_ref.ptr, wt.matrix, prim, ResolveDrawMaterial(entity));
		})
		.template add<scene::GameScene>();
}

// Largest per-axis world scale encoded in a model matrix (length of its longest basis column). Used
// to grow a primitive's unit bounding radius into world space for frustum culling.
float MaxAxisScale(const glm::mat4& m) {
	const float sx = glm::length(glm::vec3(m[0]));
	const float sy = glm::length(glm::vec3(m[1]));
	const float sz = glm::length(glm::vec3(m[2]));
	return glm::max(sx, glm::max(sy, sz));
}

// Frustum-culls a primitive (unit-mesh bounding radius `unit_radius`) and, if visible, appends a
// draw command to the shared render queue. Used by the capsule system (three sub-meshes per entity,
// non-threaded). The high-throughput instanced-primitive enqueuer inlines this logic directly so
// it can use a precomputed mesh handle and avoid the per-entity PrimitiveMesh() lookup.
void EnqueuePrimitive(
	platform::Platform& p,
	const platform::PrimitiveShape shape,
	const float unit_radius,
	const glm::mat4& model,
	const platform::DrawMaterial& material,
	RenderQueue& queue,
	RenderStats& stats,
	const CameraFrustum& cull
) {
	if (cull.valid) {
		const float radius = unit_radius * MaxAxisScale(model) * 1.1f; // small margin avoids popping
		if (!cull.frustum.ContainsSphere(glm::vec3(model[3]), radius)) {
			++stats.culled;
			return;
		}
	}
	queue.commands.push_back({.mesh = p.PrimitiveMesh(shape), .transform = model, .material = material});
	++stats.queued;
}

// Builds a DrawMaterial from inline component columns (fast path). A null `mat` yields defaults.
platform::DrawMaterial MaterialFromColumns(const Material* mat, const AlbedoMap* albedo, const ShaderMap* shader) {
	if (mat == nullptr) return {};
	return {
		.texture = albedo != nullptr ? albedo->handle : -1,
		.shader = shader != nullptr ? shader->handle : -1,
		.tint = mat->color,
		.wireframe = mat->wireframe,
		.cast_shadow = mat->cast_shadow,
		.unlit = mat->unlit,
	};
}

// Registers an OnStore system that enqueues primitive TPrim into the render queue (culled), mapping
// its component to a built-in mesh + model matrix via `to_model`. Batched/instanced at flush time.
//
// Performance: this is the hot path for scenes with tens of thousands of primitives, so it is
// written the idiomatic ECS way. It uses .run to fetch the per-frame singletons (Render3DState,
// RenderQueue, RenderStats, CameraFrustum, PlatformRef) ONCE per invocation rather than once per
// entity, and reads Material/AlbedoMap/ShaderMap as self columns (direct field access) instead of
// per-entity try_get lookups. Entities that instead use a RenderWith material entity (rare, low
// count) fall back to a per-entity relationship lookup.
//
// `zone_name` is a compile-time string used for the Tracy zone so each primitive type appears as a
// distinct named span in the timeline. `type_counter` is a pointer-to-member of RenderStats for the
// per-type queued count (e.g. &RenderStats::queued_quads) accumulated under the queue lock.
template<typename TPrim, typename ToModel>
void RegisterPrimitiveEnqueuer(
	const flecs::world& world,
	const char* name,
	const char* zone_name,
	const platform::PrimitiveShape shape,
	const float unit_radius,
	ToModel to_model,
	int RenderStats::* type_counter = nullptr
) {
	world.system<const spatial::WorldTransform, const TPrim>(name)
		.kind(flecs::OnStore)
		.template with<const Material>()
		.self()
		.optional() // field 2
		.template with<const AlbedoMap>()
		.self()
		.optional() // field 3
		.template with<const ShaderMap>()
		.self()
		.optional() // field 4
		.multi_threaded(true)
		.run([shape, unit_radius, to_model, zone_name, type_counter](flecs::iter& it) {
			profiling::Zone _zone{zone_name}; // per-primitive dynamic zone name
			const auto world = it.world();
			if (!world.get<Render3DState>().active) return;

			// Count distinct worker threads executing primitive enqueue this frame, using the
			// same generation-stamp pattern as the pathfinding thread counter.
			const int64_t cur_gen = g_render_frame_gen.load(std::memory_order_relaxed);
			if (tl_render_frame_gen != cur_gen) {
				tl_render_frame_gen = cur_gen;
				g_render_threads_active.fetch_add(1, std::memory_order_relaxed);
			}

			const auto& cull = world.get<CameraFrustum>();
			auto* platform = world.get<platform::PlatformRef>().ptr;

			// Accumulate into a thread-local buffer so concurrent system invocations never race on
			// the shared RenderQueue. One bulk-insert under a lock merges them after all entities
			// are processed, minimising lock contention to a single acquisition per system run.
			std::vector<platform::RenderCommand> local_cmds;
			int local_queued = 0;
			int local_culled = 0;

			// PrimitiveMesh maps a PrimitiveShape enum to a stable GPU mesh handle. `shape` is a
			// compile-time constant captured by the lambda, so this is the same handle every frame.
			// Call it once here rather than inside the per-entity push_back.
			const int mesh_handle = platform->PrimitiveMesh(shape);

			while (it.next()) {
				// Reserve up front so a table's worth of commands never triggers repeated
				// reallocation+copy of the (mat4-heavy) RenderCommand buffer as it grows.
				local_cmds.reserve(local_cmds.size() + static_cast<size_t>(it.count()));
				const auto wt = it.field<const spatial::WorldTransform>(0);
				const auto prim = it.field<const TPrim>(1);
				const bool has_inline = it.is_set(2);
				const bool has_albedo = it.is_set(3);
				const bool has_shader = it.is_set(4);

				// Resolve the optional column pointers once per table rather than once per entity.
				// it.field<T>(N) is a stable pointer into iter->ptrs[] for the whole table; calling
				// it inside the for loop (as it.field<T>(N)[i]) redundantly resolves it each time.
				const Material* mat_ptr = has_inline ? &it.field<const Material>(2)[0] : nullptr;
				const AlbedoMap* albedo_ptr = has_albedo ? &it.field<const AlbedoMap>(3)[0] : nullptr;
				const ShaderMap* shader_ptr = has_shader ? &it.field<const ShaderMap>(4)[0] : nullptr;

				for (const auto i : it) {
					// Compute the world-space model matrix first — the frustum check needs the
					// world center (model[3]) and scale (MaxAxisScale). Skip material resolution
					// for culled entities: avoids the column index or RenderWith lookup entirely.
					const glm::mat4 model = to_model(wt[i].matrix, prim[i]);
					if (cull.valid) {
						const float radius = unit_radius * MaxAxisScale(model) * 1.1f;
						if (!cull.frustum.ContainsSphere(glm::vec3(model[3]), radius)) {
							++local_culled;
							continue;
						}
					}

					platform::DrawMaterial material;
					if (has_inline) {
						material = MaterialFromColumns(
							mat_ptr + i,
							albedo_ptr ? albedo_ptr + i : nullptr,
							shader_ptr ? shader_ptr + i : nullptr
						);
					}
					else {
						material = MakeDrawMaterial(it.entity(i).target<RenderWith>());
					}
					local_cmds.push_back({.mesh = mesh_handle, .transform = model, .material = material});
					++local_queued;
				}
			}

			// Acquire the lock first so the get_mut dirty-mark and the vector mutations are
			// all serialised — calling get_mut from multiple worker threads before the lock
			// is a data race on Flecs' table-dirty tracking metadata.
			std::lock_guard lock(g_render_queue_mutex);
			auto& queue = world.get_mut<RenderQueue>();
			auto& stats = world.get_mut<RenderStats>();
			queue.commands.insert(
				queue.commands.end(),
				std::make_move_iterator(local_cmds.begin()),
				std::make_move_iterator(local_cmds.end())
			);
			stats.queued += local_queued;
			stats.culled += local_culled;
			if (type_counter != nullptr) {
				stats.*type_counter += local_queued;
			}
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
		c.projection = j.value("projection", std::string{"perspective"}) == "orthographic" ? Projection::Orthographic
																						   : Projection::Perspective;
		c.ortho_size = j.value("ortho_size", c.ortho_size);
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
		e.set<CapsulePrimitive>({.radius = j.value("radius", 0.5F), .height = j.value("height", 1.0F)});
	});
	level::RegisterComponentLoader(world, "mesh", [](const flecs::entity e, const json& j) {
		e.set<MeshPrimitive>({.path = assets::ResolveAsset(e.world(), j.value("path", std::string{}))});
	});
	level::RegisterComponentLoader(world, "material", [](const flecs::entity e, const json& j) {
		Material m{};
		m.color = JRgba(j, "color", m.color);
		m.wireframe = j.value("wireframe", m.wireframe);
		m.cast_shadow = j.value("cast_shadow", m.cast_shadow);
		m.unlit = j.value("unlit", m.unlit);
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

	level::RegisterComponentLoader(world, "sprite_atlas", [](const flecs::entity e, const json& j) {
		SpriteAtlas sa{};
		sa.texture_path = assets::ResolveAsset(e.world(), j.value("texture_path", std::string{}));
		sa.image_width_px = j.value("image_width_px", 1U);
		sa.image_height_px = j.value("image_height_px", 1U);
		spdlog::info("[SpriteAtlas] Set up '{}' ({}x{})", sa.texture_path, sa.image_width_px, sa.image_height_px);
		e.set<SpriteAtlas>(sa);
	});

	// "use_sprite_atlas" ref: records the UseSpriteAtlas(consumer → atlas) relationship.
	level::RegisterRef(world, "use_sprite_atlas", [](const flecs::entity self, const flecs::entity target) {
		self.add<UseSpriteAtlas>(target);
		// Wire RenderWith so the render system finds the atlas as the material source.
		// The atlas entity has Material + AlbedoMap set by SpriteAtlasAutoMaterial.
		if (!self.has<RenderWith>(flecs::Wildcard)) self.add<RenderWith>(target);
	});
}
} // namespace

RenderModule::RenderModule(const flecs::world& world) {
	world.set<Render3DState>({});
	world.set<RenderQueue>({});
	world.set<RenderStats>({});
	world.set<CameraFrustum>({});

	// === Reflection ===
	world.component<RenderWith>().add(flecs::Exclusive);
	world.component<FrustumCullRadius>().member<float>("radius");
	world.component<LookAt>().add(flecs::Exclusive);
	world.component<SpriteAtlas>()
		.member<std::string>("texture_path")
		.member<uint32_t>("image_width_px")
		.member<uint32_t>("image_height_px");
	world.component<UseSpriteAtlas>(); // not exclusive — multiple consumers can share one atlas
	world.component<Projection>()
		.constant("Perspective", Projection::Perspective)
		.constant("Orthographic", Projection::Orthographic);
	world.component<Camera>()
		.member<glm::vec3>("up")
		.member<float>("fov")
		.member<float>("aspect_ratio")
		.member<float>("near_plane")
		.member<float>("far_plane")
		.member<Projection>("projection")
		.member<float>("ortho_size");
	world.component<CubePrimitive>().member<glm::vec3>("size");
	world.component<SpherePrimitive>().member<float>("radius");
	world.component<QuadPrimitive>().member<glm::vec2>("size");
	world.component<CapsulePrimitive>().member<float>("radius").member<float>("height");
	world.component<MeshPrimitive>().member<std::string>("path").member<int>("handle");
	world.component<DynamicMesh>().member<int>("handle");
	world.component<AmbientLight>().member<core::Rgba>("color").member<float>("intensity");
	world.component<OcclusionFade>()
		.member<float>("radius")
		.member<float>("strength")
		.member<bool>("enabled");
	world.component<TransparentPass>();
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
		.member<bool>("unlit")
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
	scripting::RegisterComponentForScripts(world, world.component<SpriteAtlas>());
	scripting::RegisterComponentForScripts(world, world.component<AlbedoMap>());

	// === Level loader ===
	RegisterRenderLevelLoaders(world);

	// === Path-asset resolvers ===
	assets::RegisterPathAsset<AlbedoMap>(world, assets::AssetType::Texture);
	assets::RegisterPathAsset<MeshPrimitive>(world, assets::AssetType::Mesh);

	// === OBSERVERS ===
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

	world.observer<ShaderMap>("ResolveShaderMap")
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

	world.observer<ShaderMap>("ReleaseShaderMap")
		.event(flecs::OnRemove)
		.each([](const flecs::iter& it, size_t, const ShaderMap& sm) {
			if (sm.handle >= 0) {
				it.world().get<platform::PlatformRef>().ptr->UnloadShader(sm.handle);
			}
		});

	// OnSet only: an undeferred set fires OnAdd before the value is copied, so texture_path would be empty.
	world.observer<const SpriteAtlas>("SpriteAtlasAutoMaterial")
		.event(flecs::OnSet)
		.each([](const flecs::entity e, const SpriteAtlas& sa) {
			if (e.has<AlbedoMap>()) return; // already set up
			e.set<Material>({.color = {.r = 255, .g = 255, .b = 255, .a = 255}});
			e.set<AlbedoMap>({.path = sa.texture_path});
		});

	world.observer<>("LinkSpriteAtlasToRenderWith")
		.event(flecs::OnAdd)
		.with<UseSpriteAtlas>(flecs::Wildcard)
		.each([](const flecs::entity e) {
			if (e.has<RenderWith>(flecs::Wildcard)) return;
			e.add<RenderWith>(e.target<UseSpriteAtlas>());
		});

	// === SYSTEM: Open the 3D pass ===

	// // Camera query: the camera's own WorldTransform (eye, field 0) + Camera (field 1), then the LookAt
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

	// Sets the view and projection matrices for subsequent render calls
	std::ignore =
		world.system("Render3DBegin")
			.kind(flecs::OnStore)
			.run([camera_query](const flecs::iter& it) {
				const auto& platform_ref = it.world().get<platform::PlatformRef>();
				auto& state = it.world().get_mut<Render3DState>();
				state.active = false;

				// Reset the per-frame queue, stats and cull frustum before primitives enqueue.
				it.world().get_mut<RenderQueue>().commands.clear();
				auto& stats = it.world().get_mut<RenderStats>();
				stats.queued = 0;
				stats.culled = 0;
				stats.queued_quads = 0;
				stats.queued_cubes = 0;
				stats.queued_spheres = 0;
				stats.queued_capsules = 0;
				// Bump the render frame generation so each enqueuer thread registers itself once.
				g_render_frame_gen.fetch_add(1, std::memory_order_relaxed);
				g_render_threads_active.store(0, std::memory_order_relaxed);
				auto& cull = it.world().get_mut<CameraFrustum>();
				cull.valid = false;

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
				const float width = static_cast<float>(platform_ref.ptr->Width());
				const float height = static_cast<float>(platform_ref.ptr->Height());
				const float aspect = height > 0.0F ? width / height : cam.aspect_ratio;
				const bool orthographic = cam.projection == Projection::Orthographic;
				platform_ref.ptr->BeginMode3D({
					.position = eye.position,
					.target = aim,
					.up = cam.up,
					.fov_y = cam.fov,
					.projection = orthographic ? platform::CameraProjection::Orthographic
											   : platform::CameraProjection::Perspective,
					.ortho_size = cam.ortho_size,
				});
				state.active = true;

				// Build the cull frustum from the same view/projection the backend will use.
				const glm::mat4 view = glm::lookAt(eye.position, aim, cam.up);
				const glm::mat4 proj = BuildProjection(cam, aspect);
				cull.frustum.FromViewProj(proj * view);
				cull.valid = true;

				g_render_begin_time = std::chrono::steady_clock::now();

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

	// === SYSTEM: Upload occluder-fade focus ===
	// Derives the fade focus from the active camera's LookAt target (the thing we don't want hidden,
	// e.g. the player) so no per-frame game code is needed; the OcclusionFade singleton only tunes
	// radius/depth/strength. Pushing a disabled value when the singleton is absent clears any stale
	// fade left on the backend.
	std::ignore = world.system("RenderOcclusionFadeUpload")
					  .kind(flecs::OnStore)
					  .run([camera_query](const flecs::iter& it) {
						  const auto& platform_ref = it.world().get<platform::PlatformRef>();
						  if (!it.world().get<Render3DState>().active) return;
						  platform::OcclusionFadeParams fade; // defaults: disabled
						  const auto* cfg = it.world().try_get<OcclusionFade>();
						  if (cfg != nullptr && cfg->enabled && camera_query.is_true()) {
							  const auto camera = camera_query.first();
							  const flecs::entity target_ent = camera.target<const LookAt>();
							  const auto* target =
								  target_ent ? target_ent.try_get<const spatial::WorldTransform>() : nullptr;
							  if (target != nullptr) {
								  fade.enabled = true;
								  fade.focus = target->position;
								  fade.radius = cfg->radius;
								  fade.strength = cfg->strength;
							  }
						  }
						  platform_ref.ptr->SetOcclusionFade(fade);
					  })
					  .add<scene::GameScene>();

	// === SYSTEMS: Enqueue primitives (batched + GPU-instanced at flush) ===
	// Cube / sphere / quad map 1:1 to a built-in unit mesh; their component drives only the local
	// scale. The systems enqueue a draw command (frustum-culled) instead of issuing a draw call, so
	// thousands of identical primitives collapse into a single instanced draw at flush time.
	RegisterPrimitiveEnqueuer<CubePrimitive>(
		world,
		"RenderCubes",
		"RenderEnqueue::Cubes",
		platform::PrimitiveShape::Cube,
		0.8660254F, // unit cube half-diagonal (sqrt(3)/2)
		[](const glm::mat4& m, const CubePrimitive& cube) { return m * glm::scale(glm::mat4(1.0F), cube.size); },
		&RenderStats::queued_cubes
	);
	RegisterPrimitiveEnqueuer<SpherePrimitive>(
		world,
		"RenderSpheres",
		"RenderEnqueue::Spheres",
		platform::PrimitiveShape::Sphere,
		1.0F,
		[](const glm::mat4& m, const SpherePrimitive& sphere) {
			return m * glm::scale(glm::mat4(1.0F), glm::vec3(sphere.radius));
		},
		&RenderStats::queued_spheres
	);
	RegisterPrimitiveEnqueuer<QuadPrimitive>(
		world,
		"RenderQuads",
		"RenderEnqueue::Quads",
		platform::PrimitiveShape::Plane,
		0.7071068F, // unit plane half-diagonal (sqrt(2)/2)
		[](const glm::mat4& m, const QuadPrimitive& quad) {
			return m * glm::scale(glm::mat4(1.0F), glm::vec3(quad.size.x, 1.0F, quad.size.y));
		},
		&RenderStats::queued_quads
	);

	// Capsule is three unit meshes (cylinder body + two hemisphere caps), so it enqueues three
	// commands per entity, mirroring the backend's immediate-mode capsule assembly.
	std::ignore =
		world.system<const spatial::WorldTransform, const CapsulePrimitive>("RenderCapsules")
			.kind(flecs::OnStore)
			.with<RenderWith>(flecs::Wildcard)
			.optional()
			.self()
			.up()
			.each([](const flecs::iter& it,
					 const size_t i,
					 const spatial::WorldTransform& wt,
					 const CapsulePrimitive& cap) {
				if (!it.world().get<Render3DState>().active) return;
				auto& queue = it.world().get_mut<RenderQueue>();
				auto& stats = it.world().get_mut<RenderStats>();
				const auto& cull = it.world().get<CameraFrustum>();
				auto* p = it.world().get<platform::PlatformRef>().ptr;
				const auto mat = ResolveDrawMaterial(it.entity(i));

				const float half = cap.height * 0.5F;
				const glm::mat4 body = wt.matrix
									   * glm::translate(glm::mat4(1.0F), {0.0F, -half, 0.0F})
									   * glm::scale(glm::mat4(1.0F), {cap.radius, cap.height, cap.radius});
				const glm::mat4 top = wt.matrix
									  * glm::translate(glm::mat4(1.0F), {0.0F, half, 0.0F})
									  * glm::scale(glm::mat4(1.0F), glm::vec3(cap.radius));
				const glm::mat4 bottom = wt.matrix
										 * glm::translate(glm::mat4(1.0F), {0.0F, -half, 0.0F})
										 * glm::scale(glm::mat4(1.0F), glm::vec3(cap.radius));

				EnqueuePrimitive(*p, platform::PrimitiveShape::Cylinder, 1.4142136F, body, mat, queue, stats, cull);
				EnqueuePrimitive(*p, platform::PrimitiveShape::Sphere, 1.0F, top, mat, queue, stats, cull);
				EnqueuePrimitive(*p, platform::PrimitiveShape::Sphere, 1.0F, bottom, mat, queue, stats, cull);
				stats.queued_capsules += 3; // three sub-primitives per capsule
			})
			.add<scene::GameScene>();

	// Loaded models and dynamic meshes are few and often unique, so they keep the direct per-entity
	// draw path rather than the instanced batch.
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

	// === SYSTEM: Flush the render queue as GPU-instanced batches ===
	// Query for the transparent occluder-ghost pass: the TransparentPass-tagged upright cutout quads.
	// They were already drawn opaque by RenderDynamicMeshes (which DISCARDED the fragments covering
	// the focus, punching a hole the player shows through). This query redraws ONLY those covering
	// fragments as a depth-write-off, alpha-blended ghost so a faint silhouette remains over the
	// player. It is iterated at the end of Render3DFlush — AFTER SubmitBatch has drawn the player and
	// other opaque instanced primitives — so the blend lands over the player (guaranteed ordering,
	// independent of Flecs' intra-phase system order).
	const auto ghost_query = world.query_builder<const spatial::WorldTransform, const DynamicMesh>()
								 .with<RenderWith>(flecs::Wildcard)
								 .optional()
								 .self()
								 .up()
								 .with<TransparentPass>()
								 .build();

	std::ignore = world.system("Render3DFlush")
					  .kind(flecs::OnStore)
					  .run([ghost_query](const flecs::iter& it) {
						  profiling::Zone _zone{"Render3DFlush"};
						  auto& queue = it.world().get_mut<RenderQueue>();
						  auto& stats = it.world().get_mut<RenderStats>();
						  const auto flush_start = std::chrono::steady_clock::now();
						  stats.enqueue_ms =
							  std::chrono::duration<double, std::milli>(flush_start - g_render_begin_time).count();
						  const bool active = it.world().get<Render3DState>().active;
						  if (active && !queue.commands.empty()) {
							  const auto& platform_ref = it.world().get<platform::PlatformRef>();
							  platform::BatchStats batch;
							  {
								  profiling::Zone _batch_zone{"Render3DFlush::SubmitBatch"};
								  batch = platform_ref.ptr->SubmitBatch(queue.commands);
							  }
							  stats.draw_calls = batch.draw_calls;
							  stats.instances = batch.instances;
							  stats.group_ms = batch.group_ms;
							  stats.draw_ms = batch.draw_ms;
						  }
						  else {
							  stats.draw_calls = 0;
							  stats.instances = 0;
							  stats.group_ms = 0.0;
							  stats.draw_ms = 0.0;
						  }
						  queue.commands.clear();

						  // Transparent occluder-ghost pass — see ghost_query above. Drawn here, after the
						  // opaque batch, so the alpha blend reveals the player behind the faded quads.
						  if (active) {
							  auto& platform = *it.world().get<platform::PlatformRef>().ptr;
							  const auto& cull = it.world().get<CameraFrustum>();
							  ghost_query.each([&platform, &cull](
												   const flecs::iter& git, const size_t i, const spatial::WorldTransform& wt, const DynamicMesh& mesh
											   ) {
								  if (mesh.handle < 0) return;
								  const auto entity = git.entity(i);
								  if (const auto* fcr = entity.try_get<FrustumCullRadius>()) {
									  if (cull.valid && !cull.frustum.ContainsSphere(glm::vec3(wt.matrix[3]), fcr->radius)) {
										  return;
									  }
								  }
								  platform::DrawMaterial ghost = ResolveDrawMaterial(entity);
								  ghost.depth_write = false;     // don't occlude the player or disturb the opaque z-sort
								  ghost.fade_transparent = true; // shader renders only the faded (covering) fragments
								  platform.DrawDynamicMesh(mesh.handle, wt.matrix, ghost);
							  });
						  }
					  })
					  .add<scene::GameScene>();

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

	// === Emit render pipeline metrics ===
	// Runs PostFrame so plots reflect the fully-flushed frame state. All values are read from the
	// RenderStats singleton (already written by Render3DFlush / enqueue systems) and the platform
	// FPS/DeltaTime API, so this adds zero compute cost to the hot path. RegisterStatsEmitter owns
	// the ProfileStats entity + plumbing; ProfilerModule flushes it each frame.
	std::ignore =
		profiling::RegisterStatsEmitter(
			world,
			"RenderStatsPlot",
			[](const flecs::world& w, profiling::ProfileStats& stats) {
				const auto& rs = w.get<RenderStats>();
				const auto* platform = w.get<platform::PlatformRef>().ptr;

				// FPS and frame time — the top-level frame budget indicators.
				stats.Plot("RenderModule", "render.fps", static_cast<int64_t>(platform ? platform->Fps() : 0));
				stats.Plot(
					"RenderModule",
					"render.frame_ms",
					static_cast<double>(platform ? platform->DeltaTime() * 1000.0f : 0.0f)
				);

				// Per-type entity counts in the render queue this frame.
				stats.Plot("RenderModule", "render.queued.total", static_cast<int64_t>(rs.queued));
				stats.Plot("RenderModule", "render.queued.quads", static_cast<int64_t>(rs.queued_quads));
				stats.Plot("RenderModule", "render.queued.cubes", static_cast<int64_t>(rs.queued_cubes));
				stats.Plot("RenderModule", "render.queued.spheres", static_cast<int64_t>(rs.queued_spheres));
				stats.Plot("RenderModule", "render.queued.capsules", static_cast<int64_t>(rs.queued_capsules));
				stats.Plot("RenderModule", "render.culled", static_cast<int64_t>(rs.culled));

				// CPU timing breakdown: enqueue → group → draw.
				stats.Plot("RenderModule", "render.enqueue_ms", rs.enqueue_ms);
				stats.Plot("RenderModule", "render.group_ms", rs.group_ms);
				stats.Plot("RenderModule", "render.draw_ms", rs.draw_ms);

				// Draw calls, instance count, and thread participation.
				stats.Plot("RenderModule", "render.draw_calls", static_cast<int64_t>(rs.draw_calls));
				stats.Plot("RenderModule", "render.instances", static_cast<int64_t>(rs.instances));
				stats.Plot(
					"RenderModule",
					"render.enqueue_threads",
					g_render_threads_active.load(std::memory_order_relaxed)
				);
			}
		).add<scene::GameScene>();

	spdlog::info("[RenderModule] Registered render systems with Flecs");
}

} // namespace engine::render
