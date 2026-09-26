#include "profiling_module.hpp"

#include <fstream>
#include <utility>
#include <variant>

#include <flecs.h>
#include <spdlog/spdlog.h>

#include "engine/spatial/spatial.hpp"
#include "profiling_components.hpp"
#include "profiling_trace.hpp"

namespace engine::profiling {

flecs::entity RegisterStatsEmitter(
	const flecs::world& world,
	const char* system_name,
	std::function<void(const flecs::world&, ProfileStats&)> emit
) {
	// Dedicated stats entity owned by this emitter; flushed automatically by ProfilerFlush.
	const flecs::entity_t source = world.entity().set<ProfileStats>({}).id();
	return world.system(system_name)
		.kind(flecs::PostFrame)
		.run([source, emit = std::move(emit)](const flecs::iter& it) {
			auto* stats = it.world().entity(source).try_get_mut<ProfileStats>();
			if (!stats) {
				return;
			}
			emit(it.world(), *stats);
		});
}

ProfilerModule::ProfilerModule(const flecs::world& world) {
	// (1) Component registration. Register explicitly at init (outside readonly) so no access path
	// can trigger first-time registration while the world is readonly + multi-staged — that is a
	// structural change and aborts with INVALID_WHILE_READONLY. ProfilerConfig especially: it is
	// opt-in (never set by default), so a runtime try_get on it would otherwise register it lazily
	// during the readonly flush. No .member() reflection: both hold non-serializable runtime state
	// (std::vector<std::variant>, std::shared_ptr<std::ofstream>).
	world.component<ProfileStats>();
	world.component<ProfilerConfig>();

	// (2) Custom flush phase — runs after all PostFrame systems have pushed their entries,
	// so every producer (render, pathfinding, system stats, world) is seen in the same flush.
	const flecs::entity flush_phase = world.entity("ProfilerFlushPhase").add(flecs::Phase).depends_on(flecs::PostFrame);

	// (3) World ECS health metrics — dogfoods the same emitter helper modules use.
	RegisterStatsEmitter(world, "WorldStatsEmit", [](const flecs::world& w, ProfileStats& stats) {
		const ecs_world_info_t* info = ecs_get_world_info(w.c_ptr());
		stats.Plot("World", "world.entities", static_cast<int64_t>(w.count<spatial::Transform>()));
		stats.Plot("World", "world.tables", static_cast<int64_t>(info->table_count));
	});

	// (4) Flush: ProfileStats must be in the template so it appears as a parameter in .each().
	// Iterates every entity with ProfileStats, routes each entry to the backend via profiling::Plot
	// (which hides Tracy + the module filter) and/or the config file, then clears the list.
	// ProfilerConfig is read-only (try_get) since the world is readonly during iteration; the file is
	// opened by the OnSet observer below so it is ready before this (readonly) flush runs.
	std::ignore =
		world.system<ProfileStats>("ProfilerFlush")
			.kind(flush_phase)
			.each([](const flecs::iter& it, size_t, ProfileStats& stats) {
				if (stats.entries.empty()) {
					return;
				}

				const auto* cfg = it.world().try_get<ProfilerConfig>();
				const bool file_out = cfg && cfg->file_enabled && cfg->file && cfg->file->is_open();

				for (const auto& entry : stats.entries) {
					std::visit([&](const auto& v) { profiling::Plot(entry.module_key, entry.name, v); }, entry.value);
					if (file_out) {
						std::visit(
							[&](const auto& v) {
								*cfg->file
									<< "{\"module\":\""
									<< entry.module_key
									<< "\",\"name\":\""
									<< entry.name
									<< "\",\"value\":"
									<< v
									<< "}\n";
							},
							entry.value
						);
					}
				}

				if (file_out) {
					cfg->file->flush();
				}
				stats.entries.clear();
			});

	// (5) Open the output file when ProfilerConfig is set — avoids lazy init inside the readonly flush.
	world.observer<ProfilerConfig>("ProfilerConfigSet").event(flecs::OnSet).each([](ProfilerConfig& cfg) {
		if (cfg.file_enabled && (!cfg.file || !cfg.file->is_open())) {
			cfg.file = std::make_shared<std::ofstream>(cfg.file_path, std::ios::app);
		}
	});

	spdlog::info("[ProfilerModule] Registered profiling systems");
}

} // namespace engine::profiling
