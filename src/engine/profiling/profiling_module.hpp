#pragma once

#include <functional>

#include <flecs.h>

#include "profiling_components.hpp"

namespace engine::profiling {

// Registers the ProfilerFlushPhase (after PostFrame) and the following systems:
//   - WorldStatsEmit  (PostFrame)          — entity/table counts → ProfileStats
//   - ProfilerFlush   (ProfilerFlushPhase) — emit all ProfileStats entries to Tracy/file, then clear
//
// Any module that wants to participate either:
//   1. Calls RegisterStatsEmitter (recommended) to register a per-frame emitter, or
//   2. Manually attaches ProfileStats to an entity and pushes entries via ProfileStats::Plot().
class ProfilerModule {
public:
	explicit ProfilerModule(const flecs::world& world);
};

// Registers a PostFrame system named `system_name` that owns a dedicated ProfileStats entity and
// invokes `emit(world, stats)` once per frame. This removes the boilerplate of creating the stats
// entity, capturing its id, and null-checking each frame — a producer just writes its measurements
// into the supplied ProfileStats. The entity is flushed automatically by ProfilerFlush.
//
// Returns the system entity so the caller can scope it, e.g. `.add<scene::GameScene>()`.
//
//   profiling::RegisterStatsEmitter(world, "RenderStatsPlot",
//       [](const flecs::world& w, profiling::ProfileStats& stats) {
//           const auto& rs = w.get<render::RenderStats>();
//           stats.Plot("RenderModule", "render.fps", static_cast<int64_t>(rs.fps));
//       }).add<scene::GameScene>();
flecs::entity RegisterStatsEmitter(
	const flecs::world& world,
	const char* system_name,
	std::function<void(const flecs::world&, ProfileStats&)> emit
);

} // namespace engine::profiling
