// Render command batching: commands share an instanced draw only when everything uniform across that draw matches.

#include <vector>

#include <gtest/gtest.h>

#include "engine/platform/render_batch.hpp"

using engine::platform::DrawMaterial;
using engine::platform::GroupRenderCommands;
using engine::platform::RenderCommand;

TEST(RenderBatch, DoubleSidedAndCulledDrawsOfOneMeshDrawSeparately) {
	// Culling is set per draw call, so a double-sided quad must not ride in a culled quad's instanced draw.
	const std::vector<RenderCommand> commands{
		{.mesh = 2, .material = DrawMaterial{.double_sided = false}},
		{.mesh = 2, .material = DrawMaterial{.double_sided = true}},
		{.mesh = 2, .material = DrawMaterial{.double_sided = false}},
	};
	std::vector<uint32_t> order;

	const auto groups = GroupRenderCommands(commands, order);

	ASSERT_EQ(groups.size(), 2U);
	EXPECT_EQ(groups[0].count + groups[1].count, 3U);
	for (const auto& group : groups) {
		const bool first = commands[order[group.begin]].material.double_sided;
		for (uint32_t k = 0; k < group.count; ++k) {
			EXPECT_EQ(commands[order[group.begin + k]].material.double_sided, first);
		}
	}
}

TEST(RenderBatch, DifferentlyTintedCopiesStillShareOneDraw) {
	const std::vector<RenderCommand> commands{
		{.mesh = 0, .material = DrawMaterial{.tint = {.r = 255, .g = 0, .b = 0, .a = 255}}},
		{.mesh = 0, .material = DrawMaterial{.tint = {.r = 0, .g = 255, .b = 0, .a = 255}}},
	};
	std::vector<uint32_t> order;

	EXPECT_EQ(GroupRenderCommands(commands, order).size(), 1U);
}
