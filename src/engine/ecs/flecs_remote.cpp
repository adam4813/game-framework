#include "flecs_remote.hpp"

namespace engine::ecs {

void InitializeRemoteAPI(flecs::world& world, const int rest_port) {
	world.import<flecs::stats>();

	world.set<flecs::Rest>({
		.port = static_cast<uint16_t>(rest_port),
		.ipaddr = nullptr, // Default to 0.0.0.0
		.impl = nullptr,
	});
}

} // namespace engine::ecs
