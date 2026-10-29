#include "tilemap_scene.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include <random>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "title_scene.hpp"

using namespace engine;

namespace game {

namespace {
constexpr int MAX_NPC_COUNT = 100000;

} // namespace

void TilemapScene::RegisterLoaders(const flecs::world& world) {
	// Grid movement (player_controlled tag + grid_mover loader) is shared with the farm scene and
	// registered once at startup in game.cpp; the tilemap scene only adds its own demo loaders here.
	level::RegisterTag<BtWandererNPC>(world, "bt_wanderer");
}
void TilemapScene::InitializePipeline(const flecs::world& world) {
	// clang-format off
	pipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::MenuScene>()
					// Re-inject the built-in phase sorting logic:
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();

	pausedPipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::MenuScene>()
					.without<ecs::Pausable>()
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();
	// clang-format on
}

void TilemapScene::Load(flecs::world& world) {
	sceneRoot_ = world.entity("TilemapSceneRoot");
	world.dim(MAX_NPC_COUNT);
	spawned_npc_count_ = 0;

	world.set<render::AmbientLight>({.color = {.r = 180, .g = 180, .b = 200, .a = 255}, .intensity = 0.5F});

	SetupTilemap(world);

	world.entity("TileCallbackDemoScript")
		.child_of(tilemap_entity_)
		.set<scripting::ScriptComponent>({.source_path = assets::ResolveAsset(world, "scripts/tile_callback_demo.as")});

	SetupPlayer(world);
	SetupNPCs(world);

	// Spawner system: creates kbatchSize stress-test NPCs per frame until MAX_NPC_COUNT is reached,
	// then self-disables. No rate() throttle — 5 entities/frame keeps structural-change cost small
	// and avoids the 1000-entity spawn-spike that caused a single-frame PathRequestSystem hitch.
	auto spawner = world.system("PathfindingStressTestSpawner")
					   .kind(flecs::OnUpdate)
					   .run([this](flecs::iter& it) {
						   if (spawned_npc_count_ >= MAX_NPC_COUNT) {
							   it.system().disable();
							   return;
						   }
						   SetupPathfindingStressTest();
					   })
					   .add<ecs::Pausable>()
					   .add<scene::GameScene>();
	spawner.child_of(sceneRoot_);

	world.get<EngineContextRef>().ptr->GetDeferredTaskQueue().Enqueue([this]() { SetupPathfindingAgent(); });
	RegisterInputSystems(world);
	BuildUI(world);
	SetupDialogSystem(world);
}

void TilemapScene::Unload(flecs::world& world) {
	if (dialog_modal_) {
		dialog_modal_.destruct();
		dialog_modal_ = {};
	}
	if (world.has<DialogActiveSingleton>()) {
		world.remove<DialogActiveSingleton>();
	}

	if (sceneRoot_) {
		sceneRoot_.destruct();
		sceneRoot_ = flecs::entity{};
	}
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = flecs::entity{};
	}
	tilemap_entity_ = flecs::entity{};
	player_entity_ = flecs::entity{};
	castle_hopper_entity_ = flecs::entity{};
	pauseModal_ = flecs::entity{};
	dialog_modal_ = flecs::entity{};

	// Clear pause state when unloading the scene
	world.remove<scene::Paused>();
}

void TilemapScene::Tick(const flecs::world& world, float) {
	// Per-frame logic
	// Toggle pause modal visibility based on pause state
	const bool paused = world.has<scene::Paused>();
	if (pauseModal_ && pauseModal_.has<ui::Modal>()) {
		// Only write when the value actually changes: get_mut dirty-marks the component and notifies
		// OnSet observers every frame otherwise, for a discrete pause/unpause state.
		if (auto& modal = pauseModal_.get_mut<ui::Modal>(); modal.open != paused) {
			modal.open = !world.has<DialogActiveSingleton>() && paused;
		}
	}
}

void TilemapScene::HandleInput(const flecs::world& world, const input::InputState& input) {
	if (input.keys[input::KeyCode::Escape].pressed) {
		if (world.has<scene::Paused>()) {
			world.remove<scene::Paused>();
		}
		else {
			world.add<scene::Paused>();
		}
	}
}

void TilemapScene::RegisterInputSystems(const flecs::world& world) const {
	// The grid-movement systems are shared with the farm scene; register them scoped to this
	// scene's root so they tear down with the scene.
	tilemap::RegisterGridMovementSystems(world, sceneRoot_, tilemap_entity_);
}

void TilemapScene::SetupTilemap(const flecs::world& world) {
	// Load the tilemap entity (and all NPC/registry entities) from a single level JSON. Scene Load
	// runs undeferred, so refs are resolved before we inject C++ callbacks and explicitly bake the tilemap.
	level::LoadLevel(world, assets::ResolveAsset(world, "levels/tilemap_map.level.json"), sceneRoot_);
	tilemap_entity_ = sceneRoot_.lookup("Tilemap");
	if (!tilemap_entity_.is_valid()) {
		spdlog::error("[TilemapScene] 'Tilemap' entity not found after level load");
		return;
	}

	const flecs::entity registry_entity = tilemap_entity_.target<level::UseRegistry>();
	if (!registry_entity.is_valid()) {
		spdlog::error("[TilemapScene] Tilemap has no UseRegistry target");
		return;
	}

	// Inject C++ tile callbacks BEFORE explicit bake so they're included in the baked result.
	auto& tile_set = registry_entity.get_mut<tilemap::TileSet>();
	if (const auto it = tile_set.tiles.find(100); it != tile_set.tiles.end()) {
		it->second.onenter = [](const flecs::entity entity, int /*tile_x*/, int /*tile_z*/) {
			auto* grid_pos = entity.try_get_mut<tilemap::GridPosition>();
			if (!grid_pos) {
				spdlog::warn("[Teleporter] Entity {} has no GridPosition", entity.id());
				return;
			}
			grid_pos->x = 31; // player spawn
			grid_pos->z = 60;
			spdlog::info("[Teleporter] Entity {} teleported to ({}, {})", entity.id(), grid_pos->x, grid_pos->z);

			if (auto* mover = entity.try_get_mut<GridMover>()) {
				mover->origin = mover->target;
				mover->target = glm::vec3(
					(static_cast<float>(grid_pos->x) + 0.5f) * mover->step_size,
					mover->origin.y,
					(static_cast<float>(grid_pos->z) + 0.5f) * mover->step_size
				);
				mover->elapsed = 0.0f;
				mover->duration = 0.15f;
				mover->moving = true;
			}
		};
	}
}

void TilemapScene::SetupPlayer(const flecs::world& world) const {
	// The player, its sword and the follow-camera (a child of the player) are authored
	// declaratively in assets/levels/tilemap_player.level.json and loaded under sceneRoot_ for
	// single-destruct teardown. GridMover is default-state: the movement system seeds its origin
	// from the authored Transform, so no runtime positions need to be computed here. Runs after
	// SetupTilemap so the grid scale that the authored spawn assumes (0.5) is already in place.
	level::LoadLevel(world, assets::ResolveAsset(world, "levels/tilemap_player.level.json"), sceneRoot_);
}

void TilemapScene::SetupNPCs(const flecs::world& world) const {
	// Each NPC navigates to its associated tilemap entity via UseRegistry, avoiding a global lookup
	// that would be ambiguous with multiple tilemaps.
	const auto randomWander = [](const flecs::entity e) {
		const flecs::entity tilemap_e = e.target<level::UseRegistry>();
		if (!tilemap_e.is_valid()) {
			spdlog::warn("[NPC] Entity '{}' has no UseRegistry target", e.name() ? e.name() : "?");
			return;
		}
		const auto* tilemap = tilemap_e.try_get<tilemap::Tilemap>();
		// TileSet is on the tilemap's UseRegistry target (BasicRegistry). One hop.
		const auto* tile_set = tilemap_e.target<level::UseRegistry>().try_get<tilemap::TileSet>();
		if (!tilemap || !tile_set) return;

		auto* gridPosition = e.try_get_mut<tilemap::GridPosition>();
		auto* mover = e.try_get_mut<GridMover>();
		const auto* transform = e.try_get<spatial::Transform>();
		if (gridPosition == nullptr || mover == nullptr || transform == nullptr || mover->moving) {
			return;
		}

		const int start = static_cast<int>(e.id()) % 4;
		for (int i = 0; i < 4; ++i) {
			constexpr int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
			const int direction = (start + i) % 4;
			const int nextX = gridPosition->x + directions[direction][0];
			const int nextZ = gridPosition->z + directions[direction][1];
			if (!tilemap::InBounds(*tilemap, nextX, nextZ)) {
				continue;
			}

			const auto* descriptor = tile_set->GetTile(tilemap::GetTileIdAt(*tilemap, nextX, nextZ));
			if (descriptor != nullptr && !descriptor->walkable) {
				continue;
			}

			gridPosition->x = nextX;
			gridPosition->z = nextZ;
			mover->origin = transform->position;
			mover->target = glm::vec3(
				(static_cast<float>(nextX) + 0.5F) * mover->step_size,
				mover->origin.y,
				(static_cast<float>(nextZ) + 0.5F) * mover->step_size
			);
			mover->elapsed = 0.0F;
			mover->duration = 0.15F;
			mover->moving = true;
			break;
		}
	};

	auto& btRegistry = world.get_mut<ai::BTHandlerRegistry>();
	btRegistry.actions["RandomWander"] = [randomWander](const flecs::entity e, ai::Blackboard&, float) {
		randomWander(e);
		return ai::BTStatus::Running;
	};
	btRegistry.conditions["IsMoving"] = [](const flecs::entity e, const ai::Blackboard&) {
		const auto* mover = e.try_get<GridMover>();
		return mover != nullptr && mover->moving;
	};

	auto& utilityRegistry = world.get_mut<ai::UtilityActionRegistry>();
	utilityRegistry.handlers["Wander"] = [randomWander](const flecs::entity e, ai::UtilityAgent& agent, float) {
		randomWander(e);
		agent.context["boredom"] = 0.0F;
		agent.context["calm"] = 1.0F;
	};

	const auto fsmConditionUpdate = world.system<ai::FSMAgent, GridMover>("FSMConditionUpdate")
										.kind(flecs::OnUpdate)
										.each([randomWander](
												  const flecs::iter& it,
												  const size_t i,
												  ai::FSMAgent& fsmAgent,
												  const GridMover& gridMover
											  ) {
											fsmAgent.conditions["move_done"] = !gridMover.moving;
											if (fsmAgent.state_changed && fsmAgent.current_state == "Wander") {
												randomWander(it.entity(i));
											}
										})
										.add<ecs::Pausable>()
										.add<scene::GameScene>();
	fsmConditionUpdate.child_of(sceneRoot_);

	const auto utilityContextUpdate =
		world.system<ai::UtilityAgent>("UtilityContextUpdate")
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter& it, size_t, ai::UtilityAgent& utilityAgent) {
				const float boredom = std::clamp(utilityAgent.context["boredom"] + it.delta_time(), 0.0F, 1.0F);
				utilityAgent.context["boredom"] = boredom;
				utilityAgent.context["calm"] = 1.0F - boredom;
			})
			.add<ecs::Pausable>()
			.add<scene::GameScene>();
	utilityContextUpdate.child_of(sceneRoot_);

	// NPC entities (BtWandererNPC, FsmWandererNPC, UtilityWandererNPC) are already loaded from
	// tilemap_map.level.json by SetupTilemap — no separate LoadLevel call needed here.
}

void TilemapScene::SetupPathfindingAgent() {
	auto world = this->sceneRoot_.world();
	const auto* tilemap = tilemap_entity_.try_get<tilemap::Tilemap>();
	const flecs::entity registry_entity = tilemap_entity_.target<level::UseRegistry>();
	const auto* tileset = registry_entity.try_get<tilemap::TileSet>();
	if (!tilemap || !tileset) {
		spdlog::error("[TilemapScene] SetupPathfindingAgent: tilemap or tileset not ready");
		return;
	}

	auto& nav_env = world.get_mut<pathfinding::NavEnvironment>();
	// Register the nav graph (tilemap origin is at world (0,0,0))
	nav_env.graph = tilemap::BuildNavGraphFromTilemap(*tilemap, *tileset, glm::vec3{0.0f});
	// get_mut does not emit an event, so signal the swap explicitly; the pathfinding module observes
	// this to invalidate its path cache.
	// TODO(ux): encapsulate assign + modified in a pathfinding helper (e.g. SetNavGraph) so callers
	// can't forget the event and scenes don't need NavEnvironment/module knowledge.
	world.modified<pathfinding::NavEnvironment>();

	const auto* grid = dynamic_cast<const pathfinding::GridNavGraph*>(nav_env.graph.get());
	if (!grid) {
		spdlog::warn("[TilemapScene] StressTest: nav graph is not a GridNavGraph");
		return;
	}

	// Precompute world-space centres of all walkable tiles
	walkable.reserve(static_cast<size_t>(grid->width) * static_cast<size_t>(grid->height));
	for (int z = 0; z < grid->height; ++z) {
		for (int x = 0; x < grid->width; ++x) {
			if (!grid->IsWalkable(x, z)) continue;
			walkable.push_back({
				grid->origin.x + (static_cast<float>(x) + 0.5f) * grid->cell_size,
				grid->origin.y,
				grid->origin.z + (static_cast<float>(z) + 0.5f) * grid->cell_size,
			});
		}
	}

	// Castle hopper agent: a visible sphere that cycles NW → NE → S → NW
	castle_hopper_entity_ = world.entity("CastleHopper").child_of(sceneRoot_);

	const flecs::entity mat = world.entity()
								  .child_of(castle_hopper_entity_)
								  .set<render::Material>({
									  .color = {.r = 240, .g = 80, .b = 60, .a = 255},
									  .cast_shadow = false,
								  });

	castle_hopper_entity_.set<spatial::Transform>({.position = kCastleCenters[0]})
		.set<render::SpherePrimitive>({.radius = 0.25f})
		.add<render::RenderWith>(mat)
		.set<tilemap::GridPosition>({
			.x = static_cast<int>(kCastleCenters[0].x / 0.5f),
			.z = static_cast<int>(kCastleCenters[0].z / 0.5f),
		})
		.set<GridMover>({.step_size = 0.5f})
		.set<pathfinding::PathAgent>({.arrival_radius = 0.6f})
		.set<CastleHopper>({})
		// Initial request: head to castle NE (index 1)
		.set<pathfinding::PathRequest>({.goal = kCastleCenters[1]});

	// Drive GridMover from pathfinding waypoints, one tile step at a time
	const auto grid_path_follow_sys =
		world
			.system<pathfinding::PathAgent, GridMover, tilemap::GridPosition, const spatial::Transform>(
				"CastleHopperGridFollow"
			)
			.with<CastleHopper>()
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter&,
					 size_t,
					 pathfinding::PathAgent& agent,
					 GridMover& mover,
					 tilemap::GridPosition& grid_pos,
					 const spatial::Transform& transform) {
				if (agent.reached_goal || !agent.HasPath()) return;
				if (mover.moving) return;

				if (agent.current_waypoint >= agent.WaypointCount()) {
					agent.reached_goal = true;
					return;
				}

				const glm::vec3& target = agent.Waypoint(agent.current_waypoint);
				++agent.current_waypoint;

				grid_pos.x = static_cast<int>(target.x / mover.step_size);
				grid_pos.z = static_cast<int>(target.z / mover.step_size);

				mover.origin = transform.position;
				mover.target = target;
				const float dist = glm::length(target - transform.position);
				mover.duration = std::max(0.05f, dist * 0.25f);
				mover.elapsed = 0.0f;
				mover.moving = true;
			})
			.add<ecs::Pausable>();
	grid_path_follow_sys.child_of(sceneRoot_);

	// When a castle is reached, advance to the next one
	const auto cycle_sys =
		world.system<pathfinding::PathAgent, CastleHopper>("CastleHopperCycleSystem")
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter& it, size_t i, pathfinding::PathAgent& agent, CastleHopper& hopper) {
				if (!agent.reached_goal) return;
				hopper.current_index = (hopper.current_index + 1) % static_cast<int>(kCastleCenters.size());
				agent.reached_goal = false;
				const auto e = it.entity(i);
				e.set<pathfinding::PathRequest>({.goal = kCastleCenters[hopper.current_index]});
				e.enable<pathfinding::PathRequest>();
			})
			.add<ecs::Pausable>();
	cycle_sys.child_of(sceneRoot_);

	spdlog::info("[TilemapScene] Castle hopper pathfinding agent created");

	// Generic waypoint cycler system: works for any entity with WaypointCycler + PathAgent.
	// Advances to the next waypoint when the current path is completed.
	const auto waypoint_cycle_sys =
		world.system<pathfinding::PathAgent, WaypointCycler>("WaypointCyclerSystem")
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter& it, size_t i, pathfinding::PathAgent& agent, WaypointCycler& cycler) {
				if (!agent.reached_goal || cycler.waypoints.empty()) return;
				cycler.current_index = (cycler.current_index + 1) % cycler.waypoints.size();
				agent.reached_goal = false;
				const auto e = it.entity(i);
				e.set<pathfinding::PathRequest>({.goal = cycler.waypoints[cycler.current_index]});
				e.enable<pathfinding::PathRequest>();
			})
			.add<ecs::Pausable>()
			.add<scene::GameScene>();
	waypoint_cycle_sys.child_of(sceneRoot_);
}

void TilemapScene::SetupPathfindingStressTest() {
	auto world = this->sceneRoot_.world();

	if (walkable.empty()) {
		spdlog::error("[TilemapScene] StressTest: no walkable tiles");
		return;
	}

	constexpr int kbatchSize = 5;

	std::mt19937 rng{12345U + spawned_npc_count_ / kbatchSize}; // fixed seed for reproducibility
	std::uniform_int_distribution<size_t> pos_dist{0, walkable.size() - 1};
	std::uniform_int_distribution<int> wp_count_dist{3, 4};
	std::uniform_real_distribution<float> speed_dist{1.0f, 6.0f};
	std::uniform_real_distribution<float> radius_dist{0.08f, 0.28f};
	std::uniform_int_distribution<uint16_t> ch_dist{60, 255};

	for (int i = 0; i < kbatchSize; ++i) {
		const glm::vec3 start = walkable[pos_dist(rng)];

		const int n = wp_count_dist(rng);
		WaypointCycler cycler;
		cycler.waypoints.reserve(static_cast<size_t>(n));
		for (int j = 0; j < n; ++j) {
			cycler.waypoints.push_back(walkable[pos_dist(rng)]);
		}
		const glm::vec3 first_goal = cycler.waypoints[0];

		const core::Rgba color{
			.r = static_cast<uint8_t>(ch_dist(rng)),
			.g = static_cast<uint8_t>(ch_dist(rng)),
			.b = static_cast<uint8_t>(ch_dist(rng)),
			.a = 220
		};
		const float quad_size = radius_dist(rng);
		const float speed = speed_dist(rng);

		// Per-entity colour is carried as an inline Material component (a plain value), NOT a unique
		// RenderWith material entity. A unique relationship target per agent would give every agent a
		// distinct Flecs archetype (one table per entity) and cripple system iteration; an inline
		// component keeps all agents in a single shared table while the instanced renderer still
		// draws each in its own colour.
		const flecs::entity e = world.entity().child_of(sceneRoot_);

		e.set<spatial::Transform>({.position = start})
			.set<render::QuadPrimitive>({.size = {quad_size, quad_size}})
			.set<render::Material>({.color = color, .cast_shadow = false})
			.set<pathfinding::PathFollow>({.mode = pathfinding::PathFollowMode::Direct, .speed = speed})
			.set<pathfinding::PathAgent>({}) // pre-added so PathRequestSystem writes it as a direct field
			.set<WaypointCycler>(std::move(cycler))
			.set<pathfinding::PathRequest>({.goal = first_goal});
	}

	spawned_npc_count_ += kbatchSize;

	spdlog::info("[TilemapScene] Spawned {} of {} stress-test agents", spawned_npc_count_, MAX_NPC_COUNT);
}

void TilemapScene::SetupDialogSystem(const flecs::world& world) {
	// Register DialogNode component with Flecs reflection + scripting.
	// All four string fields are reflected so scripts can assign them directly (e.g. dn.text = "...").
	const auto dn_entity = world.component<DialogNode>()
							   .member<std::string>("text")
							   .member<std::string>("speaker")
							   .member<std::string>("choice_0_label")
							   .member<std::string>("choice_1_label");

	scripting::RegisterComponentForScripts(world, dn_entity);

	const std::vector<scripting::ScriptMethodParam> dialog_choice_params = {
		{.type = scripting::ScriptValueType::MakeObject(world.component<scripting::ScriptEntityRef>()),
		 .by_reference = false,
		 .name = "player"},
	};
	scripting::RegisterCallbackViewTypeForScripts(
		world,
		dn_entity,
		{
			{.decl = "string text", .offset = scripting::CalculateOffset(&DialogNode::text)},
			{.decl = "string speaker", .offset = scripting::CalculateOffset(&DialogNode::speaker)},
			{.decl = "string choice_0_label", .offset = scripting::CalculateOffset(&DialogNode::choice_0_label)},
			{.decl = "string choice_1_label", .offset = scripting::CalculateOffset(&DialogNode::choice_1_label)},
		},
		{
			{
				.name = "On_choice_0",
				.funcdef_name = "NodeCallback",
				.params = dialog_choice_params,
				.sink =
					[](void* object, scripting::ScriptFunctionHandle* handle) {
						auto* dn = static_cast<DialogNode*>(object);
						if (!dn) return;
						if (!handle) {
							dn->on_choice_0 = nullptr;
							return;
						}
						handle->AddRef();
						auto sp = std::shared_ptr<scripting::ScriptFunctionHandle>(
							handle,
							[](scripting::ScriptFunctionHandle* h) {
								if (h) h->Release();
							}
						);
						dn->on_choice_0 = [sp](const flecs::entity& entity) {
							scripting::ScriptEntityRef entity_ref{entity};
							const void* args[] = {&entity_ref};
							sp->Invoke(args, 1);
						};
					},
				.invoke =
					[](void* object, const scripting::ScriptCallContext& ctx) {
						const auto* dn = static_cast<DialogNode*>(object);
						const auto* player = static_cast<scripting::ScriptEntityRef*>(ctx.GetArgObject(0));
						if (!player) return;
						if (dn && dn->on_choice_0) dn->on_choice_0(player->GetEntity());
					},
			},
			{
				.name = "On_choice_1",
				.funcdef_name = "NodeCallback",
				.params = dialog_choice_params,
				.sink =
					[](void* object, scripting::ScriptFunctionHandle* handle) {
						auto* dn = static_cast<DialogNode*>(object);
						if (!dn) return;
						if (!handle) {
							dn->on_choice_1 = nullptr;
							return;
						}
						handle->AddRef();
						auto sp = std::shared_ptr<scripting::ScriptFunctionHandle>(
							handle,
							[](scripting::ScriptFunctionHandle* h) {
								if (h) h->Release();
							}
						);
						dn->on_choice_1 = [sp](const flecs::entity& entity) {
							scripting::ScriptEntityRef entity_ref{entity};
							const void* args[] = {&entity_ref};
							sp->Invoke(args, 1);
						};
					},
				.invoke =
					[](void* object, const scripting::ScriptCallContext& ctx) {
						const auto* dn = static_cast<DialogNode*>(object);
						const auto* player = static_cast<scripting::ScriptEntityRef*>(ctx.GetArgObject(0));
						if (!player) return;
						if (dn && dn->on_choice_1) dn->on_choice_1(player->GetEntity());
					},
			},
		}
	);

	// JSON loader: "dialog_node": { "text": "...", "speaker": "..." }
	level::RegisterComponentLoader(world, "dialog_node", [](const flecs::entity e, const nlohmann::json& j) {
		DialogNode dn;
		dn.text = j.value("text", std::string{});
		dn.speaker = j.value("speaker", std::string{});
		e.set<DialogNode>(std::move(dn));
	});

	// === Script global functions ===

	// ShowDialog(entity) — open dialog for the given entity's DialogNode.
	scripting::RegisterGlobalFunctionForScripts(
		world,
		{.name = "ShowDialog",
		 .return_type = scripting::ScriptValueType::MakeVoid(),
		 .params =
			 {{.type = scripting::ScriptValueType::MakeObject(world.component<scripting::ScriptEntityRef>()),
			   .by_reference = false,
			   .name = "node"}}},
		[](const scripting::ScriptCallContext& ctx, const flecs::world& w) {
			const auto* ref = static_cast<const scripting::ScriptEntityRef*>(ctx.GetArgObject(0));
			if (ref && ref->GetEntity().is_valid()) {
				w.add<scene::Paused>();
				w.set<DialogActiveSingleton>({.current_node = ref->GetEntity()});
			}
		}
	);

	// CloseDialog() — dismiss the active dialog.
	scripting::RegisterGlobalFunctionForScripts(
		world,
		{.name = "CloseDialog", .return_type = scripting::ScriptValueType::MakeVoid()},
		[](scripting::ScriptCallContext&, const flecs::world& w) {
			w.remove<scene::Paused>();
			w.remove<DialogActiveSingleton>();
		}
	);

	// CreateDialogNode(name, text) → Entity — creates entity with DialogNode + GraphNode
	scripting::RegisterGlobalFunctionForScripts(
		world,
		{.name = "CreateDialogNode",
		 .return_type = scripting::ScriptValueType::MakeObject(world.component<scripting::ScriptEntityRef>()),
		 .params =
			 {
				 {.type = scripting::ScriptValueType::MakeString(), .by_reference = true, .name = "name"},
				 {.type = scripting::ScriptValueType::MakeString(), .by_reference = true, .name = "text"},
			 }},
		[root = sceneRoot_](scripting::ScriptCallContext& ctx, const flecs::world& w) {
			const std::string name = ctx.GetArgString(0);
			const std::string text = ctx.GetArgString(1);
			const flecs::entity e = w.entity(name.c_str()).child_of(root);
			e.set<DialogNode>({.text = text});
			e.set<graph::GraphNode>({.label = text});
			new (ctx.GetReturnObjectLocation()) scripting::ScriptEntityRef{e};
		}
	);

	// ConnectDialogNodes(parent, child) — adds structural GraphEdge for visualization.
	scripting::RegisterGlobalFunctionForScripts(
		world,
		{.name = "ConnectDialogNodes",
		 .return_type = scripting::ScriptValueType::MakeVoid(),
		 .params =
			 {
				 {.type = scripting::ScriptValueType::MakeObject(world.component<scripting::ScriptEntityRef>()),
				  .by_reference = false,
				  .name = "parent"},
				 {.type = scripting::ScriptValueType::MakeObject(world.component<scripting::ScriptEntityRef>()),
				  .by_reference = false,
				  .name = "child"},
			 }},
		[](const scripting::ScriptCallContext& ctx, flecs::world&) {
			const auto* p = static_cast<const scripting::ScriptEntityRef*>(ctx.GetArgObject(0));
			const auto* c = static_cast<const scripting::ScriptEntityRef*>(ctx.GetArgObject(1));
			if (p && c) {
				p->GetEntity().add<graph::GraphEdge>(c->GetEntity());
			}
		}
	);

	// === Observers: rebuild/tear down dialog UI when DialogActiveSingleton changes ===

	std::ignore =
		world.observer<DialogActiveSingleton>("DialogActiveBuildUI")
			.event(flecs::OnSet)
			.each([this](const flecs::iter& it, size_t, DialogActiveSingleton&) { BuildDialogUI(it.world()); })
			.child_of(sceneRoot_);

	std::ignore = world.observer<DialogActiveSingleton>("DialogActiveDestroyUI")
					  .event(flecs::OnRemove)
					  .each([this](const flecs::iter&, size_t, DialogActiveSingleton) {
						  if (dialog_modal_) {
							  dialog_modal_.destruct();
							  dialog_modal_ = {};
						  }
					  })
					  .child_of(sceneRoot_);

	// Dialog script is a child of tilemap_entity_ so GetTileSet() resolves correctly.
	world.entity("TilemapDialogScript")
		.child_of(tilemap_entity_)
		.set<scripting::ScriptComponent>({.source_path = assets::ResolveAsset(world, "scripts/tilemap_dialog.as")});
}

void TilemapScene::BuildDialogUI(const flecs::world& world) {
	if (dialog_modal_) {
		dialog_modal_.destruct();
		dialog_modal_ = {};
	}
	if (!uiRoot_ || !world.has<DialogActiveSingleton>()) return;

	const flecs::entity node = world.get<DialogActiveSingleton>().current_node;
	if (!node.is_valid() || !node.has<DialogNode>()) return;

	const auto& dn = node.get<DialogNode>();

	std::vector<ui::PromptButton> buttons;

	auto wrap_callback = [](std::function<void(flecs::entity)> fn) {
		return [fn = std::move(fn)](const flecs::entity e) {
			if (fn) fn(e);
		};
	};

	if (!dn.choice_0_label.empty()) {
		buttons.push_back({.label = dn.choice_0_label, .on_click = wrap_callback(dn.on_choice_0)});
	}
	if (!dn.choice_1_label.empty()) {
		buttons.push_back({.label = dn.choice_1_label, .on_click = wrap_callback(dn.on_choice_1)});
	}
	if (buttons.empty()) {
		buttons.push_back({.label = "Close", .on_click = [](const flecs::entity e) {
							   e.world().remove<DialogActiveSingleton>();
						   }});
	}

	const auto* platform = world.get<platform::PlatformRef>().ptr;
	constexpr float w = 560.0F;
	constexpr float h = 310.0F; // extra height to accommodate wrapped dialog text
	const float x = (static_cast<float>(platform->Width()) - w) * 0.5F;
	const float y = static_cast<float>(platform->Height()) - h - 48.0F;
	const std::string title = dn.speaker.empty() ? "Dialog" : dn.speaker;

	dialog_modal_ =
		ui::CreatePrompt(world, {.x = x, .y = y, .w = w, .h = h}, title, dn.text, std::move(buttons)).child_of(uiRoot_);

	// Patch the message label: enable word-wrap and increase height so long dialog text is visible.
	// CreatePrompt layout: modal → column → [title_label, message_label, button_row]
	std::vector<flecs::entity> columnChildren;
	dialog_modal_.children([&](const flecs::entity child) {
		child.children([&](const flecs::entity grandchild) { columnChildren.push_back(grandchild); });
	});
	if (columnChildren.size() >= 2 && columnChildren[1].has<ui::Label>()) {
		columnChildren[1].get_mut<ui::UIRect>().rect.h = 120.0F;
		auto& lbl = columnChildren[1].get_mut<ui::Label>();
		lbl.wrap = true;
		lbl.align = ui::TextAlign::Left;
	}
}

void TilemapScene::BuildUI(const flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());

	uiRoot_ = world.entity("TilemapUI").child_of(sceneRoot_);
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});

	// Per-system Flecs timing overlay (toggle with F4).
	std::ignore =
		ui::CreateSystemStatsPanel(world, {.x = 20.0F, .y = 20.0F, .w = 420.0F, .h = h - 40.0F}).child_of(uiRoot_);

	// Pause menu, composed as a prompt (Modal + Stack + buttons). Starts closed; Tick opens it
	// while the scene is paused.
	pauseModal_ =
		ui::CreatePrompt(
			world,
			{.x = w / 2.0F - 200.0F, .y = h / 2.0F - 130.0F, .w = 400.0F, .h = 240.0F},
			"Paused",
			"",
			{
				{.label = "Resume", .on_click = [](const flecs::entity& e) { e.world().remove<scene::Paused>(); }},
				{.label = "Quit to Title",
				 .on_click = [](const flecs::entity& e) { scene::ActivateScene<TitleSceneTag>(e.world()); }},
			}
		)
			.set<ui::Modal>({.open = false})
			.child_of(uiRoot_);
}

} // namespace game
