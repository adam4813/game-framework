#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "engine/pathfinding/pathfinding.hpp"
#include "tilemap_components.hpp"

namespace engine::tilemap {

/// Build a GridNavGraph from a baked Tilemap and its TileSet.
///
/// Each tile's walkability comes from TileDescriptor::walkable; tiles with no descriptor default
/// to walkable. `origin` is the world-space position of cell (0,0) — typically the tilemap
/// entity's WorldTransform.position.
///
/// Usage (in a scene or observer after the Tilemap is baked):
///   auto nav = BuildNavGraphFromTilemap(tilemap, tileset, origin);
///   world.get_mut<pathfinding::NavEnvironment>().graph = std::move(nav);
[[nodiscard]] std::unique_ptr<pathfinding::GridNavGraph>
BuildNavGraphFromTilemap(const Tilemap& tilemap, const TileSet& tileset, glm::vec3 origin = {0.0f, 0.0f, 0.0f});

} // namespace engine::tilemap
