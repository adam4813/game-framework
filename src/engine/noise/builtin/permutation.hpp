#pragma once

#include <array>
#include <cstdint>
#include <numeric>
#include <utility>

#include "engine/ecs/singletons.hpp"

namespace engine::noise::builtin {

// The seeded RNG the built-in generators build their tables from. ecs::RngState is splitmix64, which gives the same
// draws on every platform. std::mt19937 with std::shuffle or a std distribution does not: the standard leaves how they
// turn the engine's output into values to the library, so libstdc++, the MSVC STL and libc++ built different tables,
// and so different noise, from one seed.
inline ecs::RngState SeededNoiseRng(const int seed) {
	return ecs::RngState{.seed = static_cast<std::uint64_t>(static_cast<std::uint32_t>(seed))};
}

// The values 0-255 in a Fisher-Yates order drawn from `rng`: the permutation a lattice noise hashes through.
inline std::array<int, 256> ShuffledPermutation(ecs::RngState& rng) {
	std::array<int, 256> p{};
	std::iota(p.begin(), p.end(), 0);
	for (int i = 255; i > 0; --i) {
		std::swap(p[i], p[rng.NextRange(0, i + 1)]);
	}
	return p;
}

} // namespace engine::noise::builtin
