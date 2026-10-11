#pragma once

#include <glm/glm.hpp>

namespace engine::noise {

/// Abstract interface for a single noise generation algorithm (e.g. Perlin, Simplex, Value).
/// Each concrete generator owns its seeded state and is registered by name with a
/// NoiseRegistry, which can switch between them at runtime.
class INoiseGenerator {
public:
	virtual ~INoiseGenerator() = default;

	/// Reseed the algorithm. Identical seeds always produce identical output.
	virtual void SetSeed(int seed) = 0;

	/// Sample 2D noise. Returns a value in approximately [-1, 1].
	[[nodiscard]] virtual float Sample2D(glm::vec2 pos) const = 0;

	/// Sample 3D noise. Returns a value in approximately [-1, 1].
	[[nodiscard]] virtual float Sample3D(glm::vec3 pos) const = 0;
};

/// Extension interface for Worley / cellular noise generators.
/// Exposes nearest and second-nearest feature-point distances in one query, enabling
/// F2, F2−F1 and other multi-distance combinations without re-running the spatial lookup.
///
/// The base INoiseGenerator already covers F1 via Sample2D/Sample3D.
/// Use this interface only when you need the additional distance values.
///
/// Named generator variants to register:
///   "worley_f1"     → Sample2D returns F1 (nearest distance)
///   "worley_f2"     → Sample2D returns F2 (second nearest)
///   "worley_f2_f1"  → Sample2D returns F2 − F1 (highlights cell borders)
class ICellularGenerator : public INoiseGenerator {
public:
	/// Returns {F1, F2} — nearest and second-nearest feature-point distances.
	[[nodiscard]] virtual glm::vec2 SampleCellular2D(glm::vec2 pos) const = 0;

	/// 3D variant of SampleCellular2D.
	[[nodiscard]] virtual glm::vec2 SampleCellular3D(glm::vec3 pos) const = 0;
};

} // namespace engine::noise
