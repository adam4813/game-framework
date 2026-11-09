#include "gltf_materials.hpp"

#include <memory>

#include "cgltf_prefixed.hpp"

namespace engine::platform {

std::vector<bool> LoadGltfDoubleSidedMaterials(const std::string& path) {
	constexpr cgltf_options options{};
	cgltf_data* raw_data = nullptr;
	if (cgltf_parse_file(&options, path.c_str(), &raw_data) != cgltf_result_success) return {};
	const std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw_data, &cgltf_free);

	std::vector<bool> flags(data->materials_count);
	for (cgltf_size i = 0; i < data->materials_count; ++i) {
		flags[i] = data->materials[i].double_sided != 0;
	}
	return flags;
}

} // namespace engine::platform
