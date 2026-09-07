#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

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

	// Uniform float in [lo, hi). Returns lo when the range is empty/inverted.
	float NextFloatRange(const float lo, const float hi) {
		if (hi <= lo) {
			return lo;
		}
		return lo + NextFloat() * (hi - lo);
	}

	// A Bernoulli trial: true with probability `p` (clamped to [0, 1]). Handy for success/turnover
	// style pass-or-fail rolls.
	bool Chance(const float p) {
		if (p <= 0.0F) {
			return false;
		}
		if (p >= 1.0F) {
			return true;
		}
		return NextFloat() < p;
	}

	// Pick an index in [0, weights.size()) with probability proportional to each weight (non-positive
	// weights count as zero). Returns -1 when the span is empty or every weight is <= 0. Used to sample
	// weighted outcome tables — loot drops, spawn tables, yardage buckets, ...
	int WeightedIndex(const std::span<const float> weights) {
		float total = 0.0F;
		int last_positive = -1;
		for (std::size_t i = 0; i < weights.size(); ++i) {
			if (weights[i] > 0.0F) {
				total += weights[i];
				last_positive = static_cast<int>(i);
			}
		}
		if (total <= 0.0F) {
			return -1;
		}
		float roll = NextFloat() * total;
		for (std::size_t i = 0; i < weights.size(); ++i) {
			if (weights[i] <= 0.0F) {
				continue;
			}
			roll -= weights[i];
			if (roll < 0.0F) {
				return static_cast<int>(i);
			}
		}
		return last_positive; // float-rounding fallback: the last positive-weight index
	}
};

} // namespace engine::ecs
