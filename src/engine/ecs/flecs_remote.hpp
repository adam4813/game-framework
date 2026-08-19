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
 * The REST server runs on the default port (27750) and processes
 * REST requests during world progression.
 */
void InitializeRemoteAPI(flecs::world& world, int rest_port = 27750);

} // namespace engine::ecs
