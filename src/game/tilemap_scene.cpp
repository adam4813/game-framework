#include "tilemap_scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "title_scene.hpp"

using namespace engine;

namespace game {

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
		world.system<GridMover, tilemap::GridPosition>("TilemapPlayerMovement")
			.with<PlayerControlled>()
			.kind(flecs::PreUpdate)
			.each([tilemap_query](const flecs::iter& it, size_t, GridMover& mover, tilemap::GridPosition& grid_pos) {
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

				// For diagonal moves, check destination and both adjacent tiles to prevent
				// shortcuts through corners
				if (dx != 0 && dz != 0) {
					if (!is_walkable(grid_pos.x + dx, grid_pos.z)
						|| !is_walkable(grid_pos.x, grid_pos.z + dz)
						|| !is_walkable(grid_pos.x + dx, grid_pos.z + dz)) {
						return;
					}
				}
				else {
					if (!is_walkable(grid_pos.x + dx, grid_pos.z + dz)) {
						return;
					}
				}

				grid_pos.x += dx;
				grid_pos.z += dz;
				mover.origin = mover.target;
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
		world.system<ecs::Transform, GridMover>("GridMovementSystem")
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter& it, size_t, ecs::Transform& transform, GridMover& mover) {
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

	// Update camera target to track the player's position
	const auto camera_sys =
		world.system<render::Camera, const ecs::WorldTransform>("CameraTargetUpdate")
			.term_at(1)
			.src("$player")
			.with<PlayerControlled>()
			.src("$player")
			.kind(flecs::OnUpdate)
			.each([](render::Camera& cam, const ecs::WorldTransform& player_wt) { cam.target = player_wt.position; })
			.add<ecs::Pausable>();
	camera_sys.child_of(sceneRoot_);
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

	// Assemble the tilemap entity. TileSet + TileRegistry are set before Tilemap so the mesh-builder
	// observer (fires on Tilemap OnSet) sees the tileset dimensions and descriptors it needs.
	tilemap_entity_ = world.entity("Tilemap")
						  .child_of(sceneRoot_)
						  .set<ecs::WorldTransform>({})
						  .set<tilemap::TileSet>(map_result->tileset.info)
						  .set<render::Material>({.color = {.r = 255, .g = 255, .b = 255, .a = 255}})
						  .set<render::AlbedoMap>({.path = map_result->tileset.info.texture_path})
						  .set<tilemap::TileRegistry>(map_result->tileset.registry)
						  .set<tilemap::Tilemap>(map_result->tilemap);
}

void TilemapScene::SetupPlayer(const flecs::world& world) {
	// Derive the per-tile world size from the tilemap's own (configurable) scale so the player's
	// stride always matches the rendered grid. Query the tilemap from the world rather than relying on
	// a stored handle; fall back to the default tile scale if no tilemap exists yet.
	float tile_scale = tilemap::TileWorldSize(tilemap::Tilemap{});
	world.query<const tilemap::Tilemap>().each([&tile_scale](const tilemap::Tilemap& tm) {
		tile_scale = tilemap::TileWorldSize(tm);
	});
	// Start at south road entrance (tile 31, 60).
	// Tile centres are at (x + 0.5) * tile_scale to match the tilemap renderer.
	constexpr int start_tile_x = 31;
	constexpr int start_tile_z = 60;
	const auto player_pos = glm::vec3(
		(static_cast<float>(start_tile_x) + 0.5f) * tile_scale,
		0.5f,
		(static_cast<float>(start_tile_z) + 0.5f) * tile_scale
	);

	player_entity_ = world.entity("Player")
						 .child_of(sceneRoot_)
						 .add<PlayerControlled>()
						 .set<tilemap::GridPosition>({.x = start_tile_x, .z = start_tile_z})
						 .set<GridMover>({
							 .origin = player_pos,
							 .target = player_pos,
							 .step_size = tile_scale,
						 })
						 .set<ecs::Transform>({
							 .position = player_pos,
							 .rotation = glm::vec3(0.0f, 0.0f, 0.0f),
							 .scale = glm::vec3(1.0f, 1.0f, 1.0f),
						 })
						 .set<ecs::WorldTransform>(ecs::MakeWorldTransform(player_pos, {}, {1.0f, 1.0f, 1.0f}))
						 .set<render::CubePrimitive>({{0.4f, 0.8f, 0.4f}})
						 .set<render::AlbedoMap>({.path = assets::ResolveAsset(world, "textures/player_sprite.png")})
						 .set<render::Material>({
							 .color = platform::colors::Accent,
							 .wireframe = false,
							 .cast_shadow = false,
						 });

	world.entity("PlayerSword")
		.child_of(player_entity_)
		.set<ecs::Transform>({
			.position = glm::vec3(0.2f, 0.0f, 0.0f),
			.rotation = glm::vec3(0.0f, 0.0f, 0.0f),
			.scale = glm::vec3(1.0f, 1.0f, 1.0f),
		})
		.set<ecs::WorldTransform>({})
		.set<render::CubePrimitive>({{0.2f, 0.05f, 0.05f}})
		.set<render::Material>({
			.color = {.r = 200, .g = 180, .b = 80, .a = 255},
			.wireframe = false,
			.cast_shadow = false,
		});

	// Camera follows the player by being a child of the player entity. The transform propagation
	// system will automatically compute its WorldTransform based on both the player's position and
	// the camera's local offset from the player. Render3DBegin reads the camera's WorldTransform.position.
	world.entity("TilemapCamera")
		.child_of(player_entity_)
		.set<render::Camera>({
			.target = glm::vec3(0.0f, 0.0f, 0.0f), // Relative to player
			.up = glm::vec3(0.0f, 0.0f, 1.0f),
			.fov = 60.0f,
			.aspect_ratio = 16.0f / 9.0f,
			.near_plane = 0.1f,
			.far_plane = 100.0f,
		})
		.set<ecs::Transform>({
			.position = glm::vec3(0.0f, 12.0f, 0.0f), // Offset from player (directly above)
			.rotation = glm::vec3(0.0f, 0.0f, 0.0f),
			.scale = glm::vec3(1.0f, 1.0f, 1.0f),
		})
		.set<ecs::WorldTransform>(ecs::MakeWorldTransform({0.0f, 12.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}));
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
