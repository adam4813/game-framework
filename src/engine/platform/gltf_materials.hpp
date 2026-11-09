#pragma once

#include <string>
#include <vector>

namespace engine::platform {

/// The glTF `materials[].doubleSided` flag of every material in a .glb/.gltf file, in file order (entry `i` is
/// material `i`). raylib's LoadModel drops the flag, so the backend reads it here. Parses the JSON only (no buffers),
/// needs no GL context, and returns an empty vector when the file is missing or fails to parse.
[[nodiscard]] std::vector<bool> LoadGltfDoubleSidedMaterials(const std::string& path);

} // namespace engine::platform
