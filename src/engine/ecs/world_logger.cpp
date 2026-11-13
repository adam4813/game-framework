#include "world_logger.hpp"

#include <spdlog/spdlog.h>

namespace engine::ecs {

void SetWorldLogger(flecs::world& world, const std::string& name) {
	const auto& defaultSinks = spdlog::default_logger_raw()->sinks();
	auto logger = std::make_shared<spdlog::logger>(name, defaultSinks.begin(), defaultSinks.end());
	logger->set_level(spdlog::default_logger_raw()->level());
	logger->flush_on(spdlog::default_logger_raw()->flush_level());
	world.set<WorldLogger>({std::move(logger)});
}

spdlog::logger& WorldLog(const flecs::world& world) {
	if (const auto* own = world.try_get<WorldLogger>(); own != nullptr && own->logger) {
		return *own->logger;
	}
	return *spdlog::default_logger_raw();
}

} // namespace engine::ecs
