#pragma once

#include <flecs.h>

namespace engine::noise {

/// Core Flecs module for the noise subsystem.
///
/// Registers FractalNoiseParams as a Flecs-meta value type (so it can be reflected
/// and used as a scripting value), and exposes the noise functions as scripting globals:
///
///   NoiseSampler@ GetNoiseSampler()
///   float Noise2D(const vec2 &in pos)
///   float Noise3D(const vec3 &in pos)
///   float FractalNoise2D(const vec2 &in pos, const FractalNoiseParams &in params)
///   float FractalNoise3D(const vec3 &in pos, const FractalNoiseParams &in params)
///   void Noise_SetSeed(int seed)
///   void Noise_SetAlgorithm(string name)
///
/// This module does NOT set an active noise algorithm — import a backend module
/// (e.g. BuiltinNoiseModule) after this one to register concrete generators.
class NoiseModule {
public:
	explicit NoiseModule(const flecs::world& world);
};

} // namespace engine::noise
