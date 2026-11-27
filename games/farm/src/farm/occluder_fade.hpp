#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "world_data.hpp"

// Occluder fade: an upright object (tree, building) standing between the camera and the player is
// drawn see-through as a whole, so the player stays visible behind it. The scene finds the
// occluders here each frame and moves them from the opaque object mesh into one mesh drawn by the
// engine's translucent pass (render::Translucent).
namespace game::farm {

// Objects up to this height (in tiles) never fade: a bush or fence in front of the player hides
// only its feet.
inline constexpr float kOccluderMinHeightTiles = 1.0F;

// Opacity of an occluding object's quads (0 = invisible, 1 = opaque).
inline constexpr float kOccluderAlpha = 0.35F;

// The camera and the body the fade keeps visible.
struct OccluderView {
	glm::vec3 viewDir{0.0F, 0.0F, 1.0F};      // camera forward (eye toward target); any length
	glm::vec3 focusCenter{0.0F};              // center of the focus body (the player), world space
	glm::vec3 focusHalfExtents{0.25F};        // half size of the focus body's box, world units
	float tileSize{0.5F};                     // world size of one tile
	float quadWidthTiles{1.0F};               // upright quad width, in tiles (UprightMeshConfig)
};

// Returns the indices into `objects`, ascending, of every object that occludes the focus: it is
// taller than kOccluderMinHeightTiles, one of its upright quads stands nearer the camera than the
// focus (compared on the ground plane, so a tall object behind the focus never counts), and that
// quad overlaps the focus box on screen. Screen space is the orthographic projection along
// `viewDir`, matching the farm's oblique follow camera. Quads are laid out as BuildUprightObjectMesh
// lays them out: one per footprint tile, centered on the tile, from the ground to the object height.
[[nodiscard]] std::vector<size_t> FindOccluders(const std::vector<WorldObject>& objects, const OccluderView& view);

} // namespace game::farm
