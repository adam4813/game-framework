#include "grid_movement.hpp"

#include <cfloat>
#include <cmath>

#include <glm/glm.hpp>

#include "engine/ecs/ecs.hpp"
#include "engine/input/input.hpp"
#include "engine/level/level.hpp"
#include "engine/scripting/scripting.hpp"
#include "engine/spatial/spatial.hpp"
#include "tilemap_components.hpp"

using namespace engine;

namespace engine::tilemap {

void RegisterGridMovementLoaders(const flecs::world& world) {
	// PlayerControlled is a pure tag, and GridMover is default-constructable runtime movement state
	// (the movement system seeds its origin from the authored Transform), so a level only needs
	// `grid_mover: {}` plus optional tuning overrides.
	level::RegisterTag<PlayerControlled>(world, "player_controlled");
	// Explicitly name the component and expose to scripting so tile callbacks can test
	// entity.HasComponent("PlayerControlled") robustly — explicit name prevents path-lookup ambiguity.
	const auto player_controlled_comp = world.component<PlayerControlled>("PlayerControlled");
	scripting::RegisterComponentForScripts(world, player_controlled_comp);
	world.component<GridMover>(); // pre-register before level loaders run (readonly-world safety)

	level::RegisterComponentLoader(world, "grid_mover", [](const flecs::entity e, const nlohmann::json& j) {
		GridMover m{};
		m.step_size = j.value("step_size", m.step_size);
		m.duration = j.value("duration", m.duration);
		m.initial_delay = j.value("initial_delay", m.initial_delay);
		e.set<GridMover>(m);
	});
}

void RegisterGridMovementSystems(
	const flecs::world& world,
	const flecs::entity sceneRoot,
	const flecs::entity tilemapEntity
) {
	// Input → initiate a grid move.
	// First press moves immediately; holding repeats after initial_delay at the move duration rate.
	const auto player_movement_sys =
		world.system<GridMover, tilemap::GridPosition, const spatial::Transform>("GridPlayerMovement")
			.with<PlayerControlled>()
			.kind(flecs::PreUpdate)
			.each([tilemapEntity](
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

				// Lambda to check if a tile is walkable (out-of-bounds + blocking world-object
				// footprints are treated as blocked; delegates to the shared tilemap walkability layer).
				auto is_walkable = [&](const int x, const int z) -> bool {
					const auto* tilemap = tilemapEntity.try_get<tilemap::Tilemap>();
					const auto* tile_set = tilemapEntity.target<level::UseRegistry>().try_get<tilemap::TileSet>();
					if (!tilemap || !tile_set) return false;
					return tilemap::IsTileWalkable(*tilemap, *tile_set, x, z);
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
	player_movement_sys.child_of(sceneRoot);

	// Smooth interpolation of Transform toward the target tile position.
	const auto grid_movement_sys =
		world.system<spatial::Transform, GridMover>("GridMoveInterpolate")
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
	grid_movement_sys.child_of(sceneRoot);

	// Sync the entity's GridPosition to its Transform (for minimap, triggers, etc.).
	// Only used if the transform was moved by something other than the GridMover (e.g. a cutscene or dialog node).
	std::ignore =
		world.system<spatial::Transform, tilemap::GridPosition>("GridMoveSync")
			.without<GridMover>()
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter&, size_t, const spatial::Transform& transform, tilemap::GridPosition& grid_pos) {
				grid_pos.x = static_cast<int>((transform.position.x / 0.5f) - 0.5f);
				grid_pos.z = static_cast<int>((transform.position.z / 0.5f) - 0.5f);
			})
			.add<ecs::Pausable>()
			.child_of(sceneRoot);
}

} // namespace engine::tilemap
