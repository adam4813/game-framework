#pragma once

#include <flecs.h>

namespace engine::noise::builtin {

/// Registers built-in noise generator factories with the NoiseRegistry.
///
/// Currently registers:
///   "perlin"  — Improved Perlin gradient noise
///   "simplex" — 2D/3D Simplex noise
///
/// Must be imported AFTER NoiseModule.
class BuiltinNoiseModule {
public:
	explicit BuiltinNoiseModule(const flecs::world& world);
};

} // namespace engine::noise::builtin
