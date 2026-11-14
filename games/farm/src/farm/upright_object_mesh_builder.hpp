#pragma once

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include <flecs.h>

#include "engine/engine.hpp"
#include "object_kind_registry.hpp"
#include "world_data.hpp"

namespace game::farm {

// Tunables for baking world-objects into upright quads. `tileSize` is the world-unit size of one
// tile (engine::tilemap::TileWorldSize); `atlasWidthPx`/`atlasHeightPx` normalise a sprite's pixel
// rect into UV space. `quadWidthTiles` controls how many tiles wide each upright quad is drawn.
// `alpha` is the opacity baked into every vertex color: 1 for the opaque object mesh, below 1 for
// the occluder mesh the translucent pass draws (see occluder_fade.hpp).
struct UprightMeshConfig {
	float tileSize{0.5F};
	float quadWidthTiles{1.0F};
	float atlasWidthPx{1.0F};
	float atlasHeightPx{1.0F};
	float alpha{1.0F};
};

// Maps an object kind's sprite name to its pixel rect within the atlas. Returns nullopt when the
// sprite is unknown, in which case the quad is textured with the whole atlas (UV 0..1).
using SpriteRectResolver = std::function<std::optional<engine::core::Rect>(std::string_view sprite)>;

// Pure CPU geometry builder (no ECS, no GPU). For every footprint tile of every object whose height
// is > 0, emits one vertical quad standing on that tile's ground centre: X spans `quadWidthTiles`
// tiles centred on the tile, Y runs from 0 (ground) to the object's height, in the fixed Z = tile
// centre plane with its front face toward +Z (the oblique follow-camera). An object's sprite is
// resolved via its ObjectKindDef (kind -> sprite); `resolveRect` then maps that sprite name to the
// atlas UVs. Height always comes from WorldObject::height, decoupled from the one-tile footprint.
[[nodiscard]] engine::tilemap::TilemapMeshData BuildUprightObjectMesh(
	const std::vector<WorldObject>& objects,
	const ObjectKindRegistry& kinds,
	const UprightMeshConfig& config,
	const SpriteRectResolver& resolveRect
);

// Component carrying the world-object collection to bake. Re-set<WorldObjectLayer>() on the carrier
// entity to rebuild the mesh with no stale geometry (an OnSet observer rebakes + re-uploads). The
// observer resolves the TileSet (via level::UseRegistry), the SpriteAtlas (via render::
// UseSpriteAtlas) and the ObjectKindRegistry singleton to texture the quads.
struct WorldObjectLayer {
	std::vector<WorldObject> objects;
	float tileSize{0.5F};
	float quadWidthTiles{1.0F};
};

// Tag for moving entities (villagers, creatures) that carry a dynamic upright quad driven by the
// existing GridMover/Transform pipeline. The sync system glides their GridMover toward whatever tile
// their GridPosition currently holds, and the render pass draws their quad at the resulting
// WorldTransform each frame.
struct UprightMover {};

// Installs the WorldObjectLayer OnSet build+upload observer and pre-registers the component types.
// Call once per world (from the farm game module) before any system runs.
class UprightObjectMeshBuilder {
public:
	static void Register(const flecs::world& world);
};

// Upload a single local-space upright quad mesh (centred on X at the origin, standing from y=0 to
// y=`heightWorld` in the Z=0 plane, front face +Z) and return its DynamicMesh handle, or -1 when no
// platform is present or the upload failed. Used for the per-entity moving-sprite quads (villagers,
// creatures), which always draw opaque.
[[nodiscard]] int UploadUprightQuadMesh(
	const flecs::world& world,
	float widthWorld,
	float heightWorld,
	std::optional<engine::core::Rect> spriteRect,
	float atlasWidthPx,
	float atlasHeightPx
);

// Register the scene-scoped system that drives every UprightMover entity's GridMover toward its
// GridPosition so its upright quad follows the grid each frame. Systems become children of
// `sceneRoot` so they are torn down on scene Unload.
void RegisterUprightMoverSystems(const flecs::world& world, flecs::entity sceneRoot);

} // namespace game::farm
