#include "tilemap_scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "title_scene.hpp"

using namespace engine;

namespace game {

void TilemapScene::RegisterLoaders(const flecs::world& world) {
	// Register the game-owned level-loader factories (the tilemap scene authors its player in JSON):
	// PlayerControlled is a pure tag, and GridMover is default-constructable runtime movement state
	// (the movement system seeds its origin from the authored Transform), so a level only needs
	// `grid_mover: {}` plus optional tuning overrides.
	level::RegisterTag<PlayerControlled>(world, "player_controlled");
	level::RegisterComponentLoader(world, "grid_mover", [](const flecs::entity e, const nlohmann::json& j) {
		GridMover m{};
		m.step_size = j.value("step_size", m.step_size);
		m.duration = j.value("duration", m.duration);
		m.initial_delay = j.value("initial_delay", m.initial_delay);
		e.set<GridMover>(m);
	});
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

	// Scene-owned singletons: set AmbientLight and DirectionalLight for this scene
	world.set<render::AmbientLight>({.color = {.r = 180, .g = 180, .b = 200, .a = 255}, .intensity = 0.5F});

	SetupTilemap(world); // creates tilemap_entity_ with its scoped TileRegistry and example tiles

	// Attach the tile callback demo script as a child of the tilemap entity so it shares
	// the same lifecycle and can access the tilemap's registry via self.GetTileRegistry().
	{
		world.entity("TileCallbackDemoScript")
			.child_of(tilemap_entity_)
			.set<scripting::ScriptComponent>(
				{.source_path = assets::ResolveAsset(world, "scripts/tile_callback_demo.as")}
			);
	}

	SetupPlayer(world);
	RegisterInputSystems(world);
	BuildUI(world);
}

void TilemapScene::Unload(flecs::world& world) {
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
	pauseModal_ = flecs::entity{};

	// Clear pause state when unloading the scene
	world.remove<scene::Paused>();
}

void TilemapScene::Tick(const flecs::world& world, float deltaTime) {
	// Per-frame logic
	// Toggle pause modal visibility based on pause state
	const bool paused = world.has<scene::Paused>();
	if (pauseModal_ && pauseModal_.has<ui::Modal>()) {
		// Only write when the value actually changes: get_mut dirty-marks the component and notifies
		// OnSet observers every frame otherwise, for a discrete pause/unpause state.
		if (auto& modal = pauseModal_.get_mut<ui::Modal>(); modal.open != paused) {
			modal.open = paused;
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

void TilemapScene::RegisterInputSystems(const flecs::world& world) {
	// The tilemap+registry query is scoped to the tilemap entity. Built once (cached) and captured,
	// rather than rebuilt on every move attempt inside the system body.
	auto tilemap_query = world.query<const tilemap::Tilemap, const tilemap::TileRegistry>();

	// Input → initiate a grid move.
	// First press moves immediately; holding repeats after initial_delay at the move duration rate.
	const auto player_movement_sys =
		world.system<GridMover, tilemap::GridPosition, const spatial::Transform>("TilemapPlayerMovement")
			.with<PlayerControlled>()
			.kind(flecs::PreUpdate)
			.each([tilemap_query](
					  const flecs::iter& it,
					  size_t,
					  GridMover& mover,
					  tilemap::GridPosition& grid_pos,
					  const spatial::Transform& transform
				  ) {
				const auto& input = it.world().get<input::InputState>();
				int dx = 0, dz = 0;
				bool just_pressed = false;

				// Check each direction independently so diagonals work (W+D, A+S, etc.)
				if (input.keys[input::KeyCode::W].pressed || input.keys[input::KeyCode::Up].pressed) {
					just_pressed = true;
				}
				if (input.keys[input::KeyCode::A].pressed || input.keys[input::KeyCode::Left].pressed) {
					just_pressed = true;
				}
				if (input.keys[input::KeyCode::D].pressed || input.keys[input::KeyCode::Right].pressed) {
					just_pressed = true;
				}
				if (input.keys[input::KeyCode::S].pressed || input.keys[input::KeyCode::Down].pressed) {
					just_pressed = true;
				}

				if (input.IsKeyDown(input::KeyCode::W) || input.IsKeyDown(input::KeyCode::Up)) {
					dz += 1;
				}
				if (input.IsKeyDown(input::KeyCode::S) || input.IsKeyDown(input::KeyCode::Down)) {
					dz -= 1;
				}
				if (input.IsKeyDown(input::KeyCode::A) || input.IsKeyDown(input::KeyCode::Left)) {
					dx += 1;
				}
				if (input.IsKeyDown(input::KeyCode::D) || input.IsKeyDown(input::KeyCode::Right)) {
					dx -= 1;
				}

				if (dx == 0 && dz == 0) {
					mover.hold_elapsed = 0.0f;
					return;
				}

				// Accumulate hold time; only reset on fresh press from idle
				if (just_pressed && mover.hold_elapsed <= FLT_EPSILON) {
					// Fresh press from idle state
					mover.hold_elapsed = 0.0f;
				}
				else {
					mover.hold_elapsed += it.delta_time();
				}

				// Can't start another move until the current one finishes
				if (mover.moving) return;

				// Held but initial delay hasn't elapsed yet — wait
				if (!just_pressed && mover.hold_elapsed < mover.initial_delay) return;

				// Lambda to check if a tile is walkable (out-of-bounds is treated as blocked).
				auto is_walkable = [&](const int x, const int z) -> bool {
					bool walkable = true;
					tilemap_query.each([&](const tilemap::Tilemap& tilemap, const tilemap::TileRegistry& registry) {
						if (!tilemap::InBounds(tilemap, x, z)) {
							walkable = false;
							return;
						}
						const auto* descriptor = registry.GetTile(tilemap::GetTileIdAt(tilemap, x, z));
						if (descriptor && !descriptor->walkable) {
							walkable = false;
						}
					});
					return walkable;
				};

				// For diagonal moves, allow sliding along walls:
				const bool target_clear = is_walkable(grid_pos.x + dx, grid_pos.z + dz);
				if (dx != 0 && dz != 0) {
					const bool x_clear = is_walkable(grid_pos.x + dx, grid_pos.z);
					const bool z_clear = is_walkable(grid_pos.x, grid_pos.z + dz);

					if (!x_clear && !z_clear) {
						return;
					}

					if (!target_clear) {
						if (x_clear) {
							dz = 0;
						}
						else if (z_clear) {
							dx = 0;
						}
					}
					else {
						if (!x_clear) {
							dx = 0;
						}
						if (!z_clear) {
							dz = 0;
						}
					}
				}
				else if (!target_clear) {
					return;
				}

				grid_pos.x += dx;
				grid_pos.z += dz;
				// Start the move from the entity's current world-local position rather than the last
				// target. This makes GridMover default-constructable (a level can author just
				// `grid_mover: {}`): the authored Transform is the spawn, and there is no jump-from-0
				// on the first move. In steady state (stationary between moves) this equals mover.target.
				mover.origin = transform.position;
				// Tile centre = (index + 0.5) * step_size, matching the tilemap renderer
				mover.target = glm::vec3(
					(static_cast<float>(grid_pos.x) + 0.5f) * mover.step_size,
					mover.origin.y,
					(static_cast<float>(grid_pos.z) + 0.5f) * mover.step_size
				);
				// Duration scales with Euclidean distance: all directions same speed
				const float distance = std::sqrt(static_cast<float>(dx * dx + dz * dz));
				mover.duration = distance * 0.15f;
				mover.elapsed = 0.0f;
				mover.moving = true;
			})
			.add<ecs::Pausable>();
	player_movement_sys.child_of(sceneRoot_);

	// Smooth interpolation of Transform toward the target tile position.
	const auto grid_movement_sys =
		world.system<spatial::Transform, GridMover>("GridMovementSystem")
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter& it, size_t, spatial::Transform& transform, GridMover& mover) {
				if (!mover.moving) return;

				mover.elapsed += it.delta_time();
				const float t = glm::clamp(mover.elapsed / mover.duration, 0.0f, 1.0f);
				transform.position = glm::mix(mover.origin, mover.target, t);

				if (t >= 1.0f) {
					mover.moving = false;
				}
			})
			.add<ecs::Pausable>();
	grid_movement_sys.child_of(sceneRoot_);
}

void TilemapScene::SetupTilemap(const flecs::world& world) {
	// Data-driven: BuildMap loads both tileset and map from JSON in one call.
	// The map's "tileset" field specifies which tileset to use (see assets/data/maps/overworld.json).
	auto map_result = tilemap::BuildMap(world, assets::ResolveAsset(world, "data/maps/overworld.json"));
	if (!map_result) {
		spdlog::error("[TilemapScene] Failed to build map");
		return;
	}

	// Inject C++ tile behaviour into the loaded tileset's registry BEFORE it is set on the entity;
	// injecting after (get_mut) would hit the scene-Load deferred-write hazard (docs/scenes.md). The
	// teleporter (id 100) draws from the tileset — we add only its onenter (return to player spawn).
	if (const auto it = map_result->tileset.registry.tiles.find(100); it != map_result->tileset.registry.tiles.end()) {
		it->second.onenter = [](const flecs::entity entity, int /*tile_x*/, int /*tile_z*/) {
			auto* grid_pos = entity.try_get_mut<tilemap::GridPosition>();
			if (!grid_pos) {
				spdlog::warn("[Teleporter] Entity {} has no GridPosition", entity.id());
				return;
			}
			grid_pos->x = 31; // player spawn
			grid_pos->z = 60;
			spdlog::info("[Teleporter] Entity {} teleported to ({}, {})", entity.id(), grid_pos->x, grid_pos->z);

			// Snap the smooth mover to the new tile so the visual follows immediately.
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

	tilemap_entity_ = world.entity("Tilemap")
						  .child_of(sceneRoot_)
						  .set<spatial::Transform>({})
						  .set<tilemap::TileSet>(map_result->tileset.info)
						  .set<tilemap::TileRegistry>(map_result->tileset.registry)
						  .set<tilemap::TilemapViewport>({})
						  .set<tilemap::Tilemap>(map_result->tilemap);

	// Material entity is a child of the tilemap entity so the TilesetTextureParams observer can
	// detect it via .with<TileSet>().self().up() (ChildOf traversal to the tilemap parent).
	tilemap_entity_.add<render::RenderWith>(
		world.entity("mat.Tilemap")
			.child_of(tilemap_entity_)
			.set<render::Material>({.color = {.r = 255, .g = 255, .b = 255, .a = 255}})
			.set<render::AlbedoMap>({.path = map_result->tileset.info.texture_path})
			.set<render::ShaderMap>(
				{.stages = {
					 {.type = "vertex", .path = "tilemap.vs"},
					 {.type = "fragment", .path = "tilemap.fs"},
				 }}
			)
	);
}

void TilemapScene::SetupPlayer(const flecs::world& world) {
	// The player, its sword and the follow-camera (a child of the player) are authored
	// declaratively in assets/levels/tilemap_player.level.json and loaded under sceneRoot_ for
	// single-destruct teardown. GridMover is default-state: the movement system seeds its origin
	// from the authored Transform, so no runtime positions need to be computed here. Runs after
	// SetupTilemap so the grid scale that the authored spawn assumes (0.5) is already in place.
	level::LoadLevel(world, assets::ResolveAsset(world, "levels/tilemap_player.level.json"), sceneRoot_);
}

void TilemapScene::BuildUI(const flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());

	uiRoot_ = world.entity("TilemapUI").child_of(sceneRoot_);
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});

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
