#pragma once

#include <cstdint>
#include <iterator>
#include <random>
#include <utility>

// Seeded draws that give the same values on every platform. std::mt19937's output is fixed by the standard, but
// std::uniform_int_distribution, std::uniform_real_distribution and std::shuffle are not: libstdc++, the MSVC STL and
// libc++ each turn the same engine output into different numbers, so a seed would make a different world on Linux,
// Windows and the web. World generation and anything derived from a seed draws through these instead.
namespace game::farm {

// A uniform int in [lo, hi]; lo when hi <= lo. Rejection sampling keeps every value equally likely.
inline int UniformInt(std::mt19937& engine, const int lo, const int hi) {
	if (hi <= lo) {
		return lo;
	}
	const std::uint64_t range = static_cast<std::uint64_t>(static_cast<std::int64_t>(hi) - lo) + 1;
	constexpr std::uint64_t kSpan = std::uint64_t{1} << 32;
	if (range >= kSpan) {
		return static_cast<int>(static_cast<std::int64_t>(lo) + engine());
	}
	// Draws at or above `limit` would favor the low remainders, so they are drawn again.
	const std::uint64_t limit = kSpan - (kSpan % range);
	std::uint64_t draw = engine();
	while (draw >= limit) {
		draw = engine();
	}
	return static_cast<int>(static_cast<std::int64_t>(lo) + static_cast<std::int64_t>(draw % range));
}

// A uniform float in [0, 1) from the engine's top 24 bits, which a float holds exactly.
inline float UniformUnitFloat(std::mt19937& engine) { return static_cast<float>(engine() >> 8) * (1.0f / 16777216.0f); }

// A uniform float in [lo, hi).
inline float UniformFloat(std::mt19937& engine, const float lo, const float hi) {
	return lo + (hi - lo) * UniformUnitFloat(engine);
}

// Fisher-Yates shuffle drawing through UniformInt, so the order is the same on every platform.
template<typename RandomIt>
void Shuffle(const RandomIt first, const RandomIt last, std::mt19937& engine) {
	const auto count = static_cast<int>(std::distance(first, last));
	for (int i = count - 1; i > 0; --i) {
		const int j = UniformInt(engine, 0, i);
		using std::swap;
		swap(first[i], first[j]);
	}
}

} // namespace game::farm
