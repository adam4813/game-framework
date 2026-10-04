#pragma once

#include <string>

namespace engine::noise {

/// World singleton tracking the active algorithm name and seed for scripting globals.
struct NoiseConfig {
	std::string active_algorithm{"perlin"};
	int seed{0};
};

} // namespace engine::noise
