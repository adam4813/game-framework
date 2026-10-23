#include "tilemap_module.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ranges>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "grid_movement.hpp"
#include "tilemap_callbacks.hpp"
#include "tilemap_components.hpp"
#include "tilemap_loader.hpp"
#include "tilemap_mesh_builder.hpp"

#include "engine/assets/assets.hpp"
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
	level::RegisterComponentLoader(world, "tileset", [](const flecs::entity e, const json& j) {
		TileSet tile_set;
		for (const auto& tile_def : j.at("tiles")) {
			TileDescriptor td{};
			td.id = tile_def.value("id", 0U);
			td.name = tile_def.value("name", std::string{});
			td.color = core::JColorNormalized(tile_def, "color", {1.0F, 1.0F, 1.0F, 1.0F});
			td.tex_coords = core::JRect(tile_def, "tex_coords");
			td.walkable = tile_def.value("walkable", true);
			tile_set.RegisterTile(td);
		}
		e.set<TileSet>(tile_set);
	});
	level::RegisterComponentLoader(world, "tilemap_spec", [](const flecs::entity e, const json& j) {
		const std::string map_path = assets::ResolveAsset(e.world(), j.value("map", std::string{}));

		const auto spec = ParseTilemapSpec(map_path);
		if (!spec) {
			spdlog::error("[TilemapSpec] Failed to parse spec from '{}'", map_path);
			return;
		}
		e.set<TilemapSpec>(*spec);
	});
	level::RegisterComponentLoader(world, "tilemap", [](const flecs::entity e, const json& j) {
		e.set<Tilemap>(TilemapFromJson(j));
	});
}
} // namespace

TilemapModule::TilemapModule(const flecs::world& world) {
	world.component<TileSet>("TileSet").add<scripting::ScriptTraversal>(world.component<level::UseRegistry>());
	world.component<Tilemap>("Tilemap");
	world.component<TilemapSpec>("TilemapSpec")
		.member<uint32_t>("width")
		.member<uint32_t>("height")
		.member<uint32_t>("tile_size_px")
		.member<float>("pixels_per_world_unit")
		.member<uint32_t>("fill_id");
	const auto td_entity = world.component<TileDescriptor>("TileDescriptor")
							   .member<uint32_t>("id")
							   .member<std::string>("name")
							   .member<glm::vec4>("color")
							   .member<std::string>("texture_path")
							   .member<bool>("walkable");
	world.component<TilemapMeshData>("TilemapMeshData");
	world.component<TileCallbackState>("TileCallbackState").member<int>("current_tile_x").member<int>("current_tile_z");
	world.component<TilemapViewport>("TilemapViewport")
		.member<int>("buffer_width")
		.member<int>("buffer_height")
		.member<int>("origin_tile_x")
		.member<int>("origin_tile_z");
	world.component<GridPosition>("GridPosition").member<int>("x").member<int>("z");
	world.component<Tilemap>()
		.add(flecs::With, world.component<TilemapViewport>())
		.add(flecs::With, world.component<spatial::Transform>());

	RegisterGridMovementLoaders(world);

	scripting::RegisterComponentForScripts(world, world.component<TileSet>());
	scripting::RegisterComponentForScripts(world, world.component<Tilemap>());
	scripting::RegisterComponentForScripts(world, world.component<TilemapSpec>());
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
	const auto color_offset = scripting::CalculateOffset(&TileDescriptor::color);
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
			{.decl = "bool walkable", .offset = scripting::CalculateOffset(&TileDescriptor::walkable)},
			{.decl = "float r", .offset = color_offset + 0 * static_cast<int>(sizeof(float))},
			{.decl = "float g", .offset = color_offset + 1 * static_cast<int>(sizeof(float))},
			{.decl = "float b", .offset = color_offset + 2 * static_cast<int>(sizeof(float))},
			{.decl = "float a", .offset = color_offset + 3 * static_cast<int>(sizeof(float))},
		},
		{{
			 .name = "OnEnter",
			 .funcdef_name = "TileEnterCallbackOnEnter",
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
				 [](void* object, const scripting::ScriptCallContext& ctx) {
					 const auto* desc = static_cast<TileDescriptor*>(object);
					 if (!desc || !desc->onenter) return;
					 const auto* player = static_cast<scripting::ScriptEntityRef*>(ctx.GetArgObject(0));
					 if (!player) return;
					 desc->onenter(player->GetEntity(), ctx.GetArgInt(1), ctx.GetArgInt(2));
				 },
		 },
		 {
			 .name = "OnLeave",
			 .funcdef_name = "TileEnterCallbackOnLeave",
			 .params = tile_enter_params,
			 .sink =
				 [](void* object, scripting::ScriptFunctionHandle* handle) {
					 auto* desc = static_cast<TileDescriptor*>(object);
					 if (!desc) return;
					 if (!handle) {
						 desc->onleave = nullptr; // td.SetOnLeave(null) clears the callback
						 return;
					 }
					 handle->AddRef();
					 const auto handle_sp = std::shared_ptr<scripting::ScriptFunctionHandle>(
						 handle,
						 [](scripting::ScriptFunctionHandle* h) {
							 if (h) h->Release();
						 }
					 );
					 desc->onleave = [handle_sp](const flecs::entity& entity, int tx, int tz) {
						 scripting::ScriptEntityRef entity_ref{entity};
						 const void* args[] = {&entity_ref, &tx, &tz};
						 handle_sp->Invoke(args, 3);
					 };
				 },
			 .invoke =
				 [](void* object, const scripting::ScriptCallContext& ctx) {
					 const auto* desc = static_cast<TileDescriptor*>(object);
					 if (!desc || !desc->onleave) return;
					 const auto* player = static_cast<scripting::ScriptEntityRef*>(ctx.GetArgObject(0));
					 if (!player) return;
					 desc->onleave(player->GetEntity(), ctx.GetArgInt(1), ctx.GetArgInt(2));
				 },
		 }}
	);

	// TileRegistry.CreateTile(id) — returns a non-owning handle to the registry's descriptor for
	// `id` (creating it if absent). The script sets fields and the onEnter callback on the handle.
	scripting::RegisterComponentMethodForScripts(
		world,
		world.component<TileSet>(),
		{
			.name = "CreateTile",
			.return_type = scripting::ScriptValueType::MakeObjectHandle(td_entity),
			.params = {{.type = scripting::ScriptValueType::MakeInt(), .by_reference = false, .name = "id"}},
			.is_const = false,
		},
		[](scripting::ScriptCallContext& ctx) {
			auto* registry = static_cast<TileSet*>(ctx.GetObject());
			if (!registry) return;
			const auto id = static_cast<uint32_t>(ctx.GetArgInt(0));
			TileDescriptor& desc = registry->tiles[id];
			desc.id = id;
			ctx.SetReturnObjectHandle(&desc);
		}
	);

	TilemapMeshBuilder::Register(world);
	TileCallbackSystem::Register(world);
	RegisterTilemapLevelLoaders(world);

	// === OBSERVERS ===
	auto tilemap_query = world.query_builder<const Tilemap, TilemapViewport>().cached().build();

	world.observer<const spatial::Transform>("TilemapComputeViewportFromCamera")
		.with<render::Camera>() // only camera entities
		.event(flecs::OnSet)
		.each([tilemap_query](const flecs::entity e, const spatial::Transform& transform) {
			const auto& camera = e.get<const render::Camera>();
			const float cam_y = std::abs(transform.position.y);
			if (cam_y <= 0.0f) return;

			tilemap_query.each([&](const flecs::entity tilemap_entity, const Tilemap& tm, TilemapViewport& vp) {
				if (vp.buffer_width > 0) return; // already sized (e.g. multiple cameras)

				const float tile_size = TileWorldSize(tm);
				int vis_w, vis_h;

				if (camera.projection == render::Projection::Orthographic) {
					// For orthographic cameras use ortho_size directly.  The oblique tilt
					// (camera.position.z != 0) projects more ground into the Z direction than
					// the flat view-height alone, so scale vis_h by a tilt factor derived from
					// the ratio of the camera's Z-offset to its height.
					const float cam_z_abs = std::abs(transform.position.z);
					const float tilt_scale = 1.0f + cam_z_abs / std::max(1.0f, cam_y);
					vis_w = static_cast<int>(std::ceil(camera.ortho_size * camera.aspect_ratio / tile_size));
					vis_h = static_cast<int>(std::ceil(camera.ortho_size * tilt_scale / tile_size));
				}
				else {
					// Perspective: coverage = 2 * cam_y * tan(half_fov)
					const float half_vfov = glm::radians(camera.fov * 0.5f);
					const float half_hfov = std::atan(camera.aspect_ratio * std::tan(half_vfov));
					vis_w = static_cast<int>(std::ceil(2.0f * cam_y * std::tan(half_hfov) / tile_size));
					vis_h = static_cast<int>(std::ceil(2.0f * cam_y * std::tan(half_vfov) / tile_size));
				}

				vp.buffer_width = std::min(vis_w + 8, static_cast<int>(tm.width));
				vp.buffer_height = std::min(vis_h + 8, static_cast<int>(tm.height));

				tilemap_entity.modified<Tilemap>(); // triggers TilemapBuildMesh with correct buffer
			});
		});

	world.observer<const TilemapSpec>("TilemapBakeOnSpec")
		.event(flecs::OnSet)
		.each([](const flecs::entity e, const TilemapSpec& spec) {
			if (e.has<Tilemap>() || spec.width == 0 || spec.height == 0) return;
			const TileSet* registry = nullptr;
			if (const flecs::entity reg_e = e.target<level::UseRegistry>(); reg_e.is_valid()) {
				registry = reg_e.try_get<TileSet>();
			}
			if (!registry) return; // defer until registry is available
			e.set<Tilemap>(BakeSpec(spec, registry));
		});

	world.observer<const TileSet>("TilemapBakeOnRegistry")
		.event(flecs::OnSet)
		.with<TilemapSpec>()
		.each([](const flecs::entity e, const TileSet& registry) {
			if (e.has<Tilemap>()) return;
			const auto* spec = e.try_get<TilemapSpec>();
			if (!spec || spec->width == 0 || spec->height == 0) return;
			e.set<Tilemap>(BakeSpec(*spec, &registry));
		});

	world.observer<>("TilemapBakeOnUseRegistry")
		.event(flecs::OnAdd)
		.with<level::UseRegistry>(flecs::Wildcard)
		.with<TilemapSpec>()
		.run([](flecs::iter& it) {
			while (it.next()) {
				for (const size_t i : it) {
					const flecs::entity e = it.entity(i);
					if (e.has<Tilemap>()) continue;
					const auto* spec = e.try_get<TilemapSpec>();
					const auto* registry = e.target<level::UseRegistry>().try_get<TileSet>();
					if (!spec || !registry || spec->width == 0 || spec->height == 0) continue;
					e.set<Tilemap>(BakeSpec(*spec, registry));
				}
			}
		});

	// === SYSTEM: Streaming update for viewport-mode tilemaps ===
	auto follow_query =
		world.query_builder<const spatial::WorldTransform>().with<TilemapFollowTarget>().cached().build();

	std::ignore =
		world
			.system<Tilemap, TilemapViewport, const TileSet, const render::DynamicMesh, spatial::Transform>(
				"TilemapStreamingUpdate"
			)
			.term_at(2)
			.self()
			.up<level::UseRegistry>()
			.kind(flecs::OnUpdate)
			.each([follow_query](
					  const flecs::iter& it,
					  size_t i,
					  const Tilemap& tm,
					  TilemapViewport& vp,
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

				const auto sprite_atlas = it.entity(i).target<render::UseSpriteAtlas>().try_get<render::SpriteAtlas>();

				// Rebuild UVs and vertex colors for all buffer slots
				const auto tex_w = static_cast<float>(sprite_atlas->image_width_px);
				const auto tex_h = static_cast<float>(sprite_atlas->image_height_px);

				std::vector<glm::vec2> uvs;
				std::vector<glm::vec4> colors;
				uvs.reserve(bw * bh * 4);
				colors.reserve(bw * bh * 4);

				for (int bz = 0; bz < bh; ++bz) {
					for (int bx = 0; bx < bw; ++bx) {
						const uint32_t tile_id = GetTileIdAt(tm, desired_x + bx, desired_z + bz);
						const auto* desc = tileset.GetTile(tile_id);

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

	spdlog::info("[TilemapModule] Registered tilemap components and systems with Flecs");
}

} // namespace engine::tilemap
