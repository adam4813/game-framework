#include "tilemap_callbacks.hpp"

#include <spdlog/spdlog.h>

#include "tilemap_components.hpp"

namespace engine::tilemap {

void TileCallbackSystem::Register(const flecs::world& world) {
	// The registry is scoped to the tilemap entity, so query it alongside the Tilemap. Built once
	// here (cached) rather than rebuilt every entity every frame inside the system body.
	auto tilemap_query = world.query<const Tilemap, const TileRegistry>();

	std::ignore =
		world.system<const GridPosition>("TileCallbackSystem")
			.kind(flecs::PostUpdate)
			.each([tilemap_query](const flecs::iter& it, const size_t i, const GridPosition& grid_pos) {
				const flecs::entity e = it.entity(i);

				if (!e.has<TileCallbackState>()) {
					e.set<TileCallbackState>({.current_tile_x = -1, .current_tile_z = -1});
				}

				auto* callback_state = e.try_get_mut<TileCallbackState>();
				if (!callback_state) return;

				// Find the current tile descriptor from the active tilemap.
				// For multi-tilemap scenes (rare), each entity should have a relationship to its tilemap;
				// for single-tilemap scenes (typical), we take the first/only result.
				const TileDescriptor* current_descriptor = nullptr;
				const Tilemap* active_tilemap = nullptr;
				const TileRegistry* active_registry = nullptr;

				tilemap_query.each([&](const Tilemap& tilemap, const TileRegistry& registry) {
					if (!active_tilemap) {
						active_tilemap = &tilemap;
						active_registry = &registry;
						current_descriptor = registry.GetTile(GetTileIdAt(tilemap, grid_pos.x, grid_pos.z));
					}
				});

				if (!active_tilemap || !active_registry) return;

				const bool entered_new_tile =
					(grid_pos.x != callback_state->current_tile_x || grid_pos.z != callback_state->current_tile_z);

				if (entered_new_tile) {
					// Fire onleave — look up the previous tile ID from the tilemap grid
					if (callback_state->current_tile_x >= 0 && callback_state->current_tile_z >= 0) {
						const TileDescriptor* prev_descriptor = active_registry->GetTile(
							GetTileIdAt(*active_tilemap, callback_state->current_tile_x, callback_state->current_tile_z)
						);
						if (prev_descriptor && prev_descriptor->onleave) {
							prev_descriptor->onleave(e, callback_state->current_tile_x, callback_state->current_tile_z);
						}
					}

					if (current_descriptor && current_descriptor->onenter) {
						current_descriptor->onenter(e, grid_pos.x, grid_pos.z);
					}

					callback_state->current_tile_x = grid_pos.x;
					callback_state->current_tile_z = grid_pos.z;
				}

				if (current_descriptor && current_descriptor->within) {
					current_descriptor->within(e, grid_pos.x, grid_pos.z);
				}
			});
}

} // namespace engine::tilemap
