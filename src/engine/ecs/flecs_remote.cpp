#include "flecs_remote.hpp"

#include <charconv>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

namespace engine::ecs {

namespace {

// An environment variable's value, or nullopt when unset. Windows' CRT deprecates getenv.
std::optional<std::string> ReadEnv(const char* name) {
#if defined(_WIN32)
	char* buffer = nullptr;
	size_t length = 0;
	if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) {
		return std::nullopt;
	}
	std::string value{buffer};
	std::free(buffer);
	return value;
#else
	const char* value = std::getenv(name);
	return value != nullptr ? std::optional<std::string>{value} : std::nullopt;
#endif
}

// The REST port: kRestPortEnv when it holds a valid port number, else `fallback`.
int ResolveRestPort(const int fallback) {
	const auto env = ReadEnv(kRestPortEnv);
	if (!env) {
		return fallback;
	}
	const std::string_view text{*env};
	int port = 0;
	const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), port);
	if (ec != std::errc{} || end != text.data() + text.size() || port <= 0 || port > 65535) {
		spdlog::warn("[FlecsRemote] Ignoring {}='{}' (not a port number); using {}", kRestPortEnv, text, fallback);
		return fallback;
	}
	return port;
}

} // namespace

void InitializeRemoteAPI(flecs::world& world, const int default_rest_port) {
	const int rest_port = ResolveRestPort(default_rest_port);
	spdlog::info("[FlecsRemote] REST API on port {}", rest_port);
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
