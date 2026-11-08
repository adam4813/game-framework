#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace engine::physics {

/// Triangle soup read from a glTF/GLB file. `indices` has three entries per triangle and indexes `positions`.
struct GltfGeometry {
	std::vector<glm::vec3> positions;
	std::vector<uint32_t> indices;

	[[nodiscard]] bool Empty() const { return indices.empty(); }
};

/// Reads every triangle primitive of every mesh in a .glb/.gltf file, with each node's world transform applied so a
/// multi-node file comes out assembled in the model's own frame (world units, no scaling beyond the file's own).
/// Needs no raylib or platform, so a headless server can build the same collision world.
/// Returns an empty result when the file is missing, fails to parse, or holds no triangles; the caller logs.
[[nodiscard]] GltfGeometry LoadGltfGeometry(const std::string& path);

} // namespace engine::physics
