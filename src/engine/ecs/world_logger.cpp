#include "world_logger.hpp"

#include <utility>

#include <spdlog/spdlog.h>

namespace engine::ecs {

void SetWorldLogger(flecs::world& world, const std::string& name) {
	const auto& fallback = *spdlog::default_logger_raw();
	auto logger = std::make_shared<spdlog::logger>(name, fallback.sinks().begin(), fallback.sinks().end());
	logger->set_level(fallback.level());
	logger->flush_on(fallback.flush_level());
	world.set<WorldLogger>({std::move(logger)});
}

spdlog::logger& WorldLog(const flecs::world& world) {
	if (const auto* own = world.try_get<WorldLogger>(); own != nullptr && own->logger) {
		return *own->logger;
	}
	return *spdlog::default_logger_raw();
}

} // namespace engine::ecs
