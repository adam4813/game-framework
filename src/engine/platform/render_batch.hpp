#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "engine/platform/platform.hpp"

// Backend-agnostic batching policy shared by every Platform backend. The GPU-specific instanced
// draw stays in each backend, but the CPU-side decision of *which commands batch together* is
// identical everywhere and lives here so backends stay thin and consistent.
namespace engine::platform {

// A contiguous run of `order` indices (see GroupRenderCommands) that share one batch key.
struct RenderBatchGroup {
	uint32_t begin{0};
	uint32_t count{0};
};

// Sorts `order` (indices into `commands`) so that commands sharing the batch key
// (mesh, shader, texture, wireframe, cast_shadow) are contiguous, and returns the resulting group
// ranges over `order`. The material tint is deliberately NOT part of the key: it is applied
// per-instance, so differently-coloured copies of the same mesh/shader/texture still batch into a
// single instanced draw. `order` is resized to match `commands`.
std::vector<RenderBatchGroup>
GroupRenderCommands(std::span<const RenderCommand> commands, std::vector<uint32_t>& order);

} // namespace engine::platform
