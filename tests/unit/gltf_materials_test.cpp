// The glTF material reader behind double-sided models: raylib's LoadModel drops `doubleSided`, so the backend reads it
// from the file itself. The test writes a tiny .gltf (JSON only, no buffers) so no asset is committed.

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/platform/gltf_materials.hpp"

namespace {

std::filesystem::path WriteGltf(const std::string& name, const std::string& json) {
	const auto path = std::filesystem::temp_directory_path() / name;
	std::ofstream(path) << json;
	return path;
}

} // namespace

TEST(GltfMaterials, EachMaterialReportsItsDoubleSidedFlagInFileOrder) {
	// Material 1 omits the flag, which the glTF spec defaults to false.
	const auto path = WriteGltf(
		"gltf_materials_test.gltf",
		R"({"asset":{"version":"2.0"},"materials":[)"
		R"({"name":"leaves","doubleSided":true},{"name":"bark"},{"name":"wall","doubleSided":false}]})"
	);

	EXPECT_EQ(engine::platform::LoadGltfDoubleSidedMaterials(path.string()), (std::vector<bool>{true, false, false}));
	std::filesystem::remove(path);
}

TEST(GltfMaterials, AMissingFileYieldsNoFlags) {
	EXPECT_TRUE(engine::platform::LoadGltfDoubleSidedMaterials("does/not/exist.glb").empty());
}
