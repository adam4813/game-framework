#include "flecs_remote.hpp"

#include <vector>

namespace engine::ecs {

void InitializeRemoteAPI(flecs::world& world, const int rest_port) {
	world.import<flecs::stats>();

	// The stats addon installs a pipeline monitor that calls ecs_pipeline_stats_get() every frame for
	// every pipeline entity. This engine runs a separate custom pipeline per scene; when a scene is
	// switched away from, its now-inactive pipeline keeps a stale execution plan while its systems go
	// inactive, and that function then asserts (`pip_count == i_system`). We don't consume pipeline
	// stats (the system-stats overlay reads per-system stats directly), so disable just the pipeline
	// monitor systems while keeping the world/system monitors the REST explorer uses. Collect first,
	// then disable, so we don't mutate entities mid-iteration.
	std::vector<flecs::entity> pipeline_monitors;
	world.component<flecs::PipelineStats>().children([&pipeline_monitors](const flecs::entity child) {
		if (child.has(flecs::System)) {
			pipeline_monitors.push_back(child);
		}
	});
	for (const flecs::entity& monitor : pipeline_monitors) {
		monitor.disable();
	}

	world.set<flecs::Rest>({
		.port = static_cast<uint16_t>(rest_port),
		.ipaddr = nullptr, // Default to 0.0.0.0
		.impl = nullptr,
	});
}

} // namespace engine::ecs
