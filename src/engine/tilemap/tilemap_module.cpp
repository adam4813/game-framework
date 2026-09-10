#include "tilemap_module.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ranges>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "tilemap_callbacks.hpp"
#include "tilemap_components.hpp"
#include "tilemap_loader.hpp"
#include "tilemap_mesh_builder.hpp"

#include "engine/assets/assets.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"
#include "engine/render/render.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"
#include "engine/spatial/spatial.hpp"

namespace engine::tilemap {

namespace {
// Registers the level-loader factories for the tilemap components this module owns. The "tilemap"
// loader is a composite filename factory: it references a map JSON and builds the TileSet, tile
// registry, viewport and grid from it in one shot (the map's own tiles/tileset stay in their data
// files). Tile behaviour callbacks are C++/script logic, so a map that needs them still wires those
// separately after load. The entity gets a local spatial::Transform (positioned by the streaming system,
// composed by TransformPropagation) so a tilemap can be parented like any other entity.
void RegisterTilemapLevelLoaders(const flecs::world& world) {
	using json = nlohmann::json;

	level::RegisterComponentLoader(world, "grid_position", [](const flecs::entity e, const json& j) {
		e.set<GridPosition>({.x = j.value("x", 0), .z = j.value("z", 0)});
	});
	level::RegisterTag<TilemapFollowTarget>(world, "tilemap_follow_target");

	level::RegisterComponentLoader(world, "tilemap", [](const flecs::entity e, const json& j) {
		auto result = BuildMap(e.world(), assets::ResolveAsset(e.world(), j.value("map", std::string{})));
		if (!result) {
			spdlog::error("[Tilemap] Level loader failed to build map '{}'", j.value("map", std::string{}));
			return;
		}
		e.set<spatial::Transform>({});
		e.set<TileSet>(result->tileset.info);
		e.set<TileRegistry>(result->tileset.registry);
		e.set<TilemapViewport>({});
		e.set<Tilemap>(result->tilemap);
	});
}
} // namespace

TilemapModule::TilemapModule(const flecs::world& world) {
	world.component<TileSet>("TileSet");
	world.component<Tilemap>("Tilemap");
	// C++ TileDescriptor: registered for Flecs meta/reflection (serialization, editor) AND exposed
	// to scripts (below) as a non-owning "TileDescriptor" view handed out by TileRegistry.CreateTile.
	const auto td_entity = world.component<TileDescriptor>("TileDescriptor")
							   .member<uint32_t>("id")
							   .member<std::string>("name")
							   .member<glm::vec4>("color")
							   .member<std::string>("texture_path")
							   .member<bool>("walkable");
	world.component<TilemapMeshData>("TilemapMeshData");
	world.component<TileRegistry>("TileRegistry");
	world.component<TileCallbackState>("TileCallbackState").member<int>("current_tile_x").member<int>("current_tile_z");
	world.component<TilemapViewport>("TilemapViewport")
		.member<int>("buffer_width")
		.member<int>("buffer_height")
		.member<int>("origin_tile_x")
		.member<int>("origin_tile_z");
	world.component<GridPosition>("GridPosition").member<int>("x").member<int>("z");

	scripting::RegisterComponentForScripts(world, world.component<TileSet>());
	scripting::RegisterComponentForScripts(world, world.component<Tilemap>());
	scripting::RegisterComponentForScripts(world, world.component<TileRegistry>());
	scripting::RegisterComponentForScripts(world, world.component<TileCallbackState>());
	scripting::RegisterComponentForScripts(world, world.component<TilemapViewport>());
	scripting::RegisterComponentForScripts(world, world.component<GridPosition>());

	// -------------------------------------------------------------------------
	// Script-facing tile authoring.
	//
	// Scripts fetch the tilemap's registry from their parent entity, ask it to create a tile
	// descriptor, then set fields and an OnEnter callback directly on that descriptor:
	//
	//   TileRegistry@ registry = self.GetTileRegistry();
	//   TileDescriptor@ td = registry.CreateTile(101);
	//   td.walkable = true; td.r = 0; td.g = 1; td.b = 1; td.a = 1;
	//   td.SetOnEnter(@OnPromptTileEnter);
	//
	// "TileDescriptor" is a non-owning view into the registry-owned C++ TileDescriptor, so
	// SetOnEnter wraps the script function straight into the descriptor's std::function — no
	// separate proxy type or stored funcdef handle (the same shape a Button's OnClick uses). The
	// invoker `td.OnEnter(player, x, z)` fires the stored callback (script- or C++-registered).
	// -------------------------------------------------------------------------

	// Expose selected TileDescriptor fields by offset. `color` is a glm::vec4 (contiguous floats),
	// so r/g/b/a map onto its four components; `id` is set by CreateTile and left read-only.
	const auto color_offset = static_cast<int>(offsetof(TileDescriptor, color));
	const std::vector<scripting::ScriptMethodParam> tile_enter_params = {
		{.type = scripting::ScriptValueType::MakeObject(world.component<scripting::ScriptEntityRef>()),
		 .by_reference = false,
		 .name = "player"},
		{.type = scripting::ScriptValueType::MakeInt(), .by_reference = false, .name = "tile_x"},
		{.type = scripting::ScriptValueType::MakeInt(), .by_reference = false, .name = "tile_z"},
	};
	scripting::RegisterCallbackViewTypeForScripts(
		world,
		td_entity,
		{
			{.decl = "bool walkable", .offset = static_cast<int>(offsetof(TileDescriptor, walkable))},
			{.decl = "float r", .offset = color_offset + 0 * static_cast<int>(sizeof(float))},
			{.decl = "float g", .offset = color_offset + 1 * static_cast<int>(sizeof(float))},
			{.decl = "float b", .offset = color_offset + 2 * static_cast<int>(sizeof(float))},
			{.decl = "float a", .offset = color_offset + 3 * static_cast<int>(sizeof(float))},
		},
		{
			{
				.name = "OnEnter",
				.funcdef_name = "TileEnterCallback",
				.params = tile_enter_params,
				.sink =
					[](void* object, scripting::ScriptFunctionHandle* handle) {
						auto* desc = static_cast<TileDescriptor*>(object);
						if (!desc) return;
						if (!handle) {
							desc->onenter = nullptr; // td.SetOnEnter(null) clears the callback
							return;
						}
						handle->AddRef();
						const auto handle_sp = std::shared_ptr<scripting::ScriptFunctionHandle>(
							handle,
							[](scripting::ScriptFunctionHandle* h) {
								if (h) h->Release();
							}
						);
						desc->onenter = [handle_sp](const flecs::entity& entity, int tx, int tz) {
							scripting::ScriptEntityRef entity_ref{entity};
							const void* args[] = {&entity_ref, &tx, &tz};
							handle_sp->Invoke(args, 3);
						};
					},
				.invoke =
					[](void* object, scripting::ScriptCallContext& ctx) {
						auto* desc = static_cast<TileDescriptor*>(object);
						if (!desc || !desc->onenter) return;
						const auto* player = static_cast<scripting::ScriptEntityRef*>(ctx.GetArgObject(0));
						if (!player) return;
						desc->onenter(player->GetEntity(), ctx.GetArgInt(1), ctx.GetArgInt(2));
					},
			},
		}
	);

	// TileRegistry.CreateTile(id) — returns a non-owning handle to the registry's descriptor for
	// `id` (creating it if absent). The script sets fields and the onEnter callback on the handle.
	scripting::RegisterComponentMethodForScripts(
		world,
		world.component<TileRegistry>(),
		{
			.name = "CreateTile",
			.return_type = scripting::ScriptValueType::MakeObjectHandle(td_entity),
			.params = {{.type = scripting::ScriptValueType::MakeInt(), .by_reference = false, .name = "id"}},
			.is_const = false,
		},
		[](scripting::ScriptCallContext& ctx) {
			auto* registry = static_cast<TileRegistry*>(ctx.GetObject());
			if (!registry) return;
			const auto id = static_cast<uint32_t>(ctx.GetArgInt(0));
			TileDescriptor& desc = registry->tiles[id];
			desc.id = id;
			ctx.SetReturnObjectHandle(&desc);
		}
	);

	// Tile callbacks are std::functions holding script-function handles, so they are released
	// naturally when a TileRegistry (its tile map) is destroyed — e.g. on scene unload, while the
	// script engine is still alive. This shutdown hook covers the *final* world teardown instead:
	// at ecs_fini the AngelScript backend may be released before the tilemap entity, so clear the
	// handles here (deterministically, before ShutDownAndRelease) to avoid releasing a script
	// function through a dead engine. A TileRegistry destructor could not guarantee this ordering.
	scripting::RegisterScriptShutdownCallback(world, [](flecs::world& shutdown_world) {
		shutdown_world.query<TileRegistry>().each([](flecs::entity, TileRegistry& registry) {
			for (auto& desc : registry.tiles | std::views::values) {
				desc.onenter = nullptr;
				desc.onleave = nullptr;
				desc.within = nullptr;
			}
		});
	});

	// Register tilemap mesh-building observers
	TilemapMeshBuilder::Register(world);

	// === OBSERVER: Apply NEAREST + CLAMP_TO_EDGE to tileset atlas textures ===
	// With the RenderWith pattern, AlbedoMap lives on a material entity that is a child of the
	// tilemap entity (which has TileSet). The .self().up() on TileSet matches entities whose
	// own or ChildOf-ancestor entity has TileSet — no manual parent traversal needed.
	// NEAREST prevents bilinear bleeding across atlas boundaries; CLAMP_TO_EDGE stops wrapping.
	world.observer<const render::AlbedoMap>("TilesetTextureParams")
		.with<TileSet>()
		.self()
		.up()
		.event(flecs::OnSet)
		.each([](const flecs::iter& it, size_t, const render::AlbedoMap& albedo) {
			if (albedo.handle < 0) return;
			auto& platform_ref = it.world().get<platform::PlatformRef>();
			platform_ref.ptr->SetTextureFilter(albedo.handle, false); // NEAREST
			platform_ref.ptr->SetTextureWrap(albedo.handle, true);    // CLAMP_TO_EDGE
		});

	// === OBSERVER: Compute TilemapViewport buffer size from camera configuration ===
	// Fires when Transform is set on a camera entity (which already has render::Camera set).
	// Reads the actual camera's FOV, aspect ratio, and height to size the streaming buffer to cover
	// the full 3D viewport plus a few tiles of padding. Triggers a Tilemap mesh rebuild so the
	// buffer-sized mesh is built before the first streaming update.
	{
		auto tilemap_query = world.query_builder<const Tilemap, TilemapViewport>().cached().build();

		world.observer<const spatial::Transform>("TilemapComputeViewportFromCamera")
			.with<render::Camera>() // only camera entities
			.event(flecs::OnSet)
			.each([tilemap_query](const flecs::entity e, const spatial::Transform& transform) {
				const auto& camera = e.get<const render::Camera>();
				const float cam_y = std::abs(transform.position.y);
				if (cam_y <= 0.0f) return;

				const float half_vfov = glm::radians(camera.fov * 0.5f);
				const float half_hfov = std::atan(camera.aspect_ratio * std::tan(half_vfov));

				tilemap_query.each([&](const flecs::entity tilemap_entity, const Tilemap& tm, TilemapViewport& vp) {
					if (vp.buffer_width > 0) return; // already sized (e.g. multiple cameras)

					const float tile_size = TileWorldSize(tm);
					const int vis_w = static_cast<int>(std::ceil(2.0f * cam_y * std::tan(half_hfov) / tile_size));
					const int vis_h = static_cast<int>(std::ceil(2.0f * cam_y * std::tan(half_vfov) / tile_size));
					vp.buffer_width = std::min(vis_w + 4, static_cast<int>(tm.width));
					vp.buffer_height = std::min(vis_h + 4, static_cast<int>(tm.height));

					tilemap_entity.modified<Tilemap>(); // triggers TilemapBuildMesh with correct buffer
				});
			});
	}

	// === SYSTEM: Streaming update for viewport-mode tilemaps ===
	// Centers on the TilemapFollowTarget entity: when it crosses a tile boundary, repositions the
	// fixed ring-buffer mesh via its local Transform and refreshes the buffer's UVs+colors to show the
	// tiles at the new origin. With no follow target it simply doesn't move (the mesh is still built by
	// the observer pipeline). Skips until the buffer is sized by the camera observer above.
	auto follow_query = world.query_builder<const spatial::WorldTransform>().with<TilemapFollowTarget>().cached().build();

	std::ignore =
		world
			.system<
				Tilemap,
				TilemapViewport,
				const TileRegistry,
				const TileSet,
				const render::DynamicMesh,
				spatial::Transform>("TilemapStreamingUpdate")
			.kind(flecs::OnUpdate)
			.each([follow_query](
					  const flecs::iter& it,
					  size_t,
					  const Tilemap& tm,
					  TilemapViewport& vp,
					  const TileRegistry& registry,
					  const TileSet& tileset,
					  const render::DynamicMesh& dyn_mesh,
					  spatial::Transform& transform
				  ) {
				if (vp.buffer_width <= 0 || vp.buffer_height <= 0) return; // waiting for camera observer

				const auto target = follow_query.first();
				// Hacky fallback: if no follow target, center on the map's middle tile (0,0) is bottom-left, +X right, +Z forward.
				const glm::vec3 follow_pos = target ? target.get<const spatial::WorldTransform>().position
													: glm::vec3{tm.width / 2.0, 0, tm.height / 2.0};

				const float tile_size = TileWorldSize(tm);
				const int map_w = static_cast<int>(tm.width);
				const int map_h = static_cast<int>(tm.height);
				const int bw = vp.buffer_width;
				const int bh = vp.buffer_height;

				// Desired origin: camera tile centre-aligned, clamped to map bounds
				int desired_x = static_cast<int>(follow_pos.x / tile_size) - bw / 2;
				int desired_z = static_cast<int>(follow_pos.z / tile_size) - bh / 2;
				desired_x = std::clamp(desired_x, 0, std::max(0, map_w - bw));
				desired_z = std::clamp(desired_z, 0, std::max(0, map_h - bh));

				if (desired_x == vp.origin_tile_x && desired_z == vp.origin_tile_z) {
					return;
				}

				vp.origin_tile_x = desired_x;
				vp.origin_tile_z = desired_z;

				// Reposition the mesh: slot (0,0) lives at world (origin * tile_size)
				const glm::vec3 origin_pos{
					static_cast<float>(desired_x) * tile_size,
					0.0f,
					static_cast<float>(desired_z) * tile_size
				};
				// Reposition the mesh by writing only the local Transform; TransformPropagation
				// (PreStore) composes it with any parent into WorldTransform, so the tilemap can be
				// parented like any other entity instead of pinning its world matrix here.
				transform.position = origin_pos;

				// Rebuild UVs and vertex colors for all buffer slots
				const auto tex_w = static_cast<float>(tileset.image_width_px);
				const auto tex_h = static_cast<float>(tileset.image_height_px);

				std::vector<glm::vec2> uvs;
				std::vector<glm::vec4> colors;
				uvs.reserve(static_cast<size_t>(bw * bh * 4));
				colors.reserve(static_cast<size_t>(bw * bh * 4));

				for (int bz = 0; bz < bh; ++bz) {
					for (int bx = 0; bx < bw; ++bx) {
						const uint32_t tile_id = GetTileIdAt(tm, desired_x + bx, desired_z + bz);
						const auto* desc = registry.GetTile(tile_id);

						glm::vec2 uv0{0.0f}, uv1{0.0f};
						glm::vec4 color{1.0f};
						if (desc) {
							color = desc->color;
							if (desc->tex_coords.w > 0.0f && desc->tex_coords.h > 0.0f) {
								uv0 = {(desc->tex_coords.x + 0.5f) / tex_w, (desc->tex_coords.y + 0.5f) / tex_h};
								uv1 = {
									(desc->tex_coords.x + desc->tex_coords.w - 0.5f) / tex_w,
									(desc->tex_coords.y + desc->tex_coords.h - 0.5f) / tex_h
								};
							}
						}

						uvs.emplace_back(uv0.x, uv0.y); // TL
						uvs.emplace_back(uv1.x, uv0.y); // TR
						uvs.emplace_back(uv1.x, uv1.y); // BR
						uvs.emplace_back(uv0.x, uv1.y); // BL

						for (int v = 0; v < 4; ++v) colors.push_back(color);
					}
				}

				const auto& platform_ref = it.world().get<platform::PlatformRef>();
				platform_ref.ptr->UpdateDynamicMeshUVs(dyn_mesh.handle, uvs);
				platform_ref.ptr->UpdateDynamicMeshColors(dyn_mesh.handle, colors);
			})
			.add<scene::GameScene>();

	// Register tile callback system
	TileCallbackSystem::Register(world);

	RegisterTilemapLevelLoaders(world);

	spdlog::info("[TilemapModule] Registered tilemap components and systems with Flecs");
}

} // namespace engine::tilemap
