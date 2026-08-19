#pragma once

#include <flecs.h>

namespace engine::tilemap {

// Bakes a Tilemap's tile grid into CPU mesh data and uploads it as a render::DynamicMesh.
// This module owns geometry generation only — it never issues draw calls. The render module
// draws whatever DynamicMesh it produces, keeping tilemap state isolated from rendering.
class TilemapMeshBuilder {
public:
	// Register the tilemap mesh-building observers with Flecs.
	static void Register(const flecs::world& world);
};

} // namespace engine::tilemap
