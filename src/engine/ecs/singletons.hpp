#pragma once

#include <cstdint>

// Singletons hold global run state. Stored on the Flecs world via world.set<T>().
// Kept as aggregates (no user-declared constructors) so `set<T>({})` works.
namespace engine::ecs {

// Deterministic RNG (splitmix64) so runs are reproducible from a seed.
struct RngState {
	uint64_t seed = 0xC0FFEEULL;

	uint64_t Next() {
		seed += 0x9E3779B97F4A7C15ULL;
		uint64_t z = seed;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		return z ^ (z >> 31);
	}

	// [0, 1)
	float NextFloat() {
		return static_cast<float>(static_cast<double>(Next() >> 40) / static_cast<double>(1ULL << 24));
	}

	// [lo, hi)
	int NextRange(const int lo, const int hi) {
		if (hi <= lo) {
			return lo;
		}
		return lo + static_cast<int>(Next() % static_cast<uint64_t>(hi - lo));
	}
};

} // namespace engine::ecs
