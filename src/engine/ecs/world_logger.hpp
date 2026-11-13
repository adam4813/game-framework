#pragma once

#include <memory>
#include <string>

#include <flecs.h>
#include <spdlog/logger.h>

namespace engine::ecs {

// Singleton: the world's own named logger, so every line a world writes says which world wrote it. Set it with
// SetWorldLogger and read it with WorldLog; a world without one logs through spdlog's default logger.
struct WorldLogger {
	std::shared_ptr<spdlog::logger> logger;
};

/**
 * Gives `world` a logger named `name` (say "client" or "server" for two worlds in one process).
 *
 * The logger writes to the default logger's sinks, so every world's lines land interleaved, in order, in the same
 * console and log file. It takes the default logger's level and flush level as they are now. The name shows wherever
 * the sinks' pattern has `%n`, which spdlog's default pattern does. Formatting belongs to the sinks, shared with the
 * default logger, so set the pattern there, never on a world's logger. The logger isn't registered with spdlog, so two
 * worlds (or two processes' worth of tests) may share a name. Calling it again replaces the world's logger.
 */
void SetWorldLogger(flecs::world& world, const std::string& name);

/**
 * The logger code holding `world` writes through: the world's own logger, or spdlog's default logger when the world
 * has none. Code with no world in reach keeps using `spdlog::info` and friends.
 *
 * Safe inside a system (`it.world()` is a stage). EngineContext registers WorldLogger; a bare world that calls this
 * from a multi-threaded system registers it first (`world.component<WorldLogger>()`).
 */
spdlog::logger& WorldLog(const flecs::world& world);

} // namespace engine::ecs
