#include "tilemap_module.hpp"

#include <cstddef>
#include <memory>
#include <ranges>
#include <spdlog/spdlog.h>

#include "tilemap_callbacks.hpp"
#include "tilemap_components.hpp"
#include "tilemap_mesh_builder.hpp"

#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"

namespace engine::tilemap {

TilemapModule::TilemapModule(const flecs::world& world) {
	// Register tilemap components
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

	world.component<GridPosition>("GridPosition").member<int>("x").member<int>("z");
	scripting::RegisterComponentForScripts(world, world.component<GridPosition>());

	// Expose tile components to scripts
	scripting::RegisterComponentForScripts(world, world.component<TileCallbackState>());

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

	scripting::RegisterComponentForScripts(world, world.component<TileRegistry>());

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

	// PauseScene() / ResumeScene() — scene control from scripts.
	scripting::RegisterGlobalFunctionForScripts(
		world,
		{.name = "PauseScene", .return_type = scripting::ScriptValueType::MakeVoid(), .params = {}},
		[&world](scripting::ScriptCallContext&, flecs::world&) { world.add<scene::Paused>(); }
	);

	scripting::RegisterGlobalFunctionForScripts(
		world,
		{.name = "ResumeScene", .return_type = scripting::ScriptValueType::MakeVoid(), .params = {}},
		[&world](scripting::ScriptCallContext&, flecs::world&) { world.remove<scene::Paused>(); }
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

	// Register tile callback system
	TileCallbackSystem::Register(world);

	spdlog::info("[TilemapModule] Registered tilemap components and systems with Flecs");
}

} // namespace engine::tilemap
