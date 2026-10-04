#pragma once

#include "noise_generator.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace engine::noise {

struct NoiseSampler;

/// Flecs world singleton. Stores named generator factories (move-only, non-copyable).
/// GetSampler creates a fresh isolated generator each call — no shared mutable state.
struct NoiseRegistry {
	NoiseRegistry() = default;
	~NoiseRegistry() = default;
	NoiseRegistry(NoiseRegistry&&) = default;
	NoiseRegistry& operator=(NoiseRegistry&&) = default;
	NoiseRegistry(const NoiseRegistry&) = delete;
	NoiseRegistry& operator=(const NoiseRegistry&) = delete;

	/// Register a factory function that creates a fresh INoiseGenerator instance.
	void Register(std::string name, std::function<std::unique_ptr<INoiseGenerator>()> factory);

	/// True if a factory with this name is registered.
	[[nodiscard]] bool Has(std::string_view name) const;

	/// Create a fresh NoiseSampler for the named generator, seeded with `seed`.
	/// Returns a null sampler if the name is not registered.
	[[nodiscard]] NoiseSampler GetSampler(std::string_view name, int seed = 0) const;

private:
	std::unordered_map<std::string, std::function<std::unique_ptr<INoiseGenerator>()>> factories_;
};

} // namespace engine::noise
