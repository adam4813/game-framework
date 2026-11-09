#include "gltf_geometry.hpp"

#include <memory>

#include <glm/gtc/type_ptr.hpp>

// The engine's prefixed cgltf copy is implemented in this translation unit; every other user includes the header alone.
#define CGLTF_IMPLEMENTATION
#include "engine/platform/cgltf_prefixed.hpp"

namespace engine::physics {

namespace {

struct CgltfDeleter {
	void operator()(cgltf_data* data) const { cgltf_free(data); }
};

void AppendPrimitive(const cgltf_primitive& prim, const glm::mat4& world, GltfGeometry& out) {
	if (prim.type != cgltf_primitive_type_triangles) return;

	const cgltf_accessor* position_accessor = nullptr;
	for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
		if (prim.attributes[a].type == cgltf_attribute_type_position && prim.attributes[a].index == 0) {
			position_accessor = prim.attributes[a].data;
			break;
		}
	}
	if (!position_accessor || position_accessor->count == 0) return;

	std::vector<float> raw(position_accessor->count * 3);
	if (cgltf_accessor_unpack_floats(position_accessor, raw.data(), raw.size()) == 0) return;

	const auto base = static_cast<uint32_t>(out.positions.size());
	for (cgltf_size v = 0; v < position_accessor->count; ++v) {
		out.positions.emplace_back(world * glm::vec4(raw[v * 3], raw[v * 3 + 1], raw[v * 3 + 2], 1.0F));
	}

	// Non-indexed primitives list triangles in vertex order.
	const cgltf_size index_count = prim.indices ? prim.indices->count : position_accessor->count;
	for (cgltf_size i = 0; i + 2 < index_count; i += 3) {
		cgltf_size tri[3];
		bool valid = true;
		for (cgltf_size k = 0; k < 3; ++k) {
			tri[k] = prim.indices ? cgltf_accessor_read_index(prim.indices, i + k) : i + k;
			valid = valid && tri[k] < position_accessor->count; // a malformed file must not index past the vertices
		}
		if (!valid) continue;
		for (const cgltf_size idx : tri) {
			out.indices.push_back(base + static_cast<uint32_t>(idx));
		}
	}
}

} // namespace

GltfGeometry LoadGltfGeometry(const std::string& path) {
	constexpr cgltf_options options{};
	cgltf_data* raw_data = nullptr;
	if (cgltf_parse_file(&options, path.c_str(), &raw_data) != cgltf_result_success) return {};
	const std::unique_ptr<cgltf_data, CgltfDeleter> data(raw_data);
	if (cgltf_load_buffers(&options, data.get(), path.c_str()) != cgltf_result_success) return {};

	GltfGeometry geometry;
	for (cgltf_size n = 0; n < data->nodes_count; ++n) {
		const cgltf_node& node = data->nodes[n];
		if (!node.mesh) continue;

		float matrix[16];
		cgltf_node_transform_world(&node, matrix);
		const glm::mat4 world = glm::make_mat4(matrix);
		for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p) {
			AppendPrimitive(node.mesh->primitives[p], world, geometry);
		}
	}
	return geometry;
}

} // namespace engine::physics
