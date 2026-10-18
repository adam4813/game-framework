#pragma once

#include <flecs.h>

namespace engine::ecs {

/**
 * Initializes the Flecs Remote API for the explorer.
 *
 * This sets up:
 * - Statistics gathering (ecs.stats)
 * - REST server for the Flecs Explorer
 *
 * Must be called after world creation but before the main loop.
 * The REST server runs on `default_rest_port` (27750) unless the FLECS_REST_PORT environment
 * variable names another, so several game instances can run side by side. It processes REST
 * requests during world progression.
 */
void InitializeRemoteAPI(flecs::world& world, int default_rest_port = 27750);

// Environment variable that overrides the REST port (`tools/flecs-api.js` reads FLECS_HOST to match).
inline constexpr const char* kRestPortEnv = "FLECS_REST_PORT";

} // namespace engine::ecs
