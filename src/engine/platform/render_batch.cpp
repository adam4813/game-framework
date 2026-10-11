#include "render_batch.hpp"

#include <algorithm>
#include <numeric>
#include <tuple>

namespace engine::platform {

namespace {
// The batch key: everything that must be uniform across an instanced draw. tint is excluded so
// per-instance colour does not fragment batches.
auto BatchKey(const RenderCommand& c) {
	return std::tuple{c.mesh, c.material.shader, c.material.texture, c.material.wireframe, c.material.cast_shadow};
}
} // namespace

std::vector<RenderBatchGroup>
GroupRenderCommands(const std::span<const RenderCommand> commands, std::vector<uint32_t>& order) {
	order.resize(commands.size());
	std::iota(order.begin(), order.end(), 0U);

	std::ranges::sort(order, [&](const uint32_t a, const uint32_t b) {
		return BatchKey(commands[a]) < BatchKey(commands[b]);
	});

	std::vector<RenderBatchGroup> groups;
	for (uint32_t i = 0; i < order.size();) {
		uint32_t j = i + 1;
		while (j < order.size() && BatchKey(commands[order[j]]) == BatchKey(commands[order[i]])) {
			++j;
		}
		groups.push_back({.begin = i, .count = j - i});
		i = j;
	}
	return groups;
}

} // namespace engine::platform
