// The CPU glTF loader behind mesh colliders. It builds a tiny .glb at test time (a translated quad) so no
// binary asset is committed, then checks the triangles come out assembled in the model's frame.

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/physics/gltf_geometry.hpp"

namespace {

void AppendU32(std::string& out, const uint32_t value) { out.append(reinterpret_cast<const char*>(&value), 4); }

// One mesh of two triangles (a unit quad in the XY plane) on a node translated by (10, 0, 0).
std::string MakeQuadGlb() {
	std::string bin;
	for (const float v : {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F}) {
		bin.append(reinterpret_cast<const char*>(&v), sizeof(float));
	}
	for (const uint16_t i : {0, 1, 2, 0, 2, 3}) {
		bin.append(reinterpret_cast<const char*>(&i), sizeof(uint16_t));
	}

	std::string json =
		R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)"
		R"("nodes":[{"mesh":0,"translation":[10,0,0]}],)"
		R"("meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"
		R"("buffers":[{"byteLength":60}],)"
		R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":12}],)"
		R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},)"
		R"({"bufferView":1,"componentType":5123,"count":6,"type":"SCALAR"}]})";
	while (json.size() % 4 != 0) json.push_back(' ');

	std::string glb;
	AppendU32(glb, 0x46546C67); // "glTF"
	AppendU32(glb, 2);
	AppendU32(glb, static_cast<uint32_t>(12 + 8 + json.size() + 8 + bin.size()));
	AppendU32(glb, static_cast<uint32_t>(json.size()));
	AppendU32(glb, 0x4E4F534A); // "JSON"
	glb += json;
	AppendU32(glb, static_cast<uint32_t>(bin.size()));
	AppendU32(glb, 0x004E4942); // "BIN\0"
	glb += bin;
	return glb;
}

} // namespace

TEST(GltfGeometry, ALoadedModelYieldsEveryTriangleInTheNodesFrame) {
	const auto path = std::filesystem::temp_directory_path() / "gltf_geometry_test_quad.glb";
	{
		std::ofstream file(path, std::ios::binary);
		const std::string glb = MakeQuadGlb();
		file.write(glb.data(), static_cast<std::streamsize>(glb.size()));
	}

	const auto geometry = engine::physics::LoadGltfGeometry(path.string());
	std::filesystem::remove(path);

	ASSERT_EQ(geometry.indices.size(), 6U);
	ASSERT_EQ(geometry.positions.size(), 4U);
	// The node's translation is applied: the quad spans x in [10, 11].
	EXPECT_FLOAT_EQ(geometry.positions[0].x, 10.0F);
	EXPECT_FLOAT_EQ(geometry.positions[2].x, 11.0F);
	EXPECT_FLOAT_EQ(geometry.positions[2].y, 1.0F);
	EXPECT_EQ(geometry.indices[3], 0U);
	EXPECT_EQ(geometry.indices[5], 3U);
}

TEST(GltfGeometry, AMissingFileYieldsNoGeometry) {
	EXPECT_TRUE(engine::physics::LoadGltfGeometry("does/not/exist.glb").Empty());
}
