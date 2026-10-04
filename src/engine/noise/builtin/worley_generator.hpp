#pragma once

#include <array>

#include "../noise_generator.hpp"

namespace engine::noise::builtin {

/// Which distance to return from Sample2D/Sample3D.
enum class WorkleyMode {
	F1,        ///< Nearest feature-point distance (default)
	F2,        ///< Second-nearest distance
	F2MinusF1, ///< F2 − F1 — sharp cell-border highlights
	F1PlusF2,  ///< F1 + F2 — smoother combined variant
};

/// Worley / Cellular noise (Steve Worley, 1996).
/// Implements ICellularGenerator so callers who only need F1 get it via the base
/// Sample2D/Sample3D interface, while callers who need both distances can call
/// SampleCellular2D/3D.
///
/// Registered under multiple names in BuiltinNoiseModule:
///   "worley_f1"     — Sample returns F1
///   "worley_f2"     — Sample returns F2
///   "worley_f2_f1"  — Sample returns F2 − F1 (cell borders)
class WorkleyGenerator final : public ICellularGenerator {
public:
	explicit WorkleyGenerator(WorkleyMode mode = WorkleyMode::F1, int seed = 0);

	void SetSeed(int seed) override;

	/// Returns the configured distance metric in [-1, 1].
	[[nodiscard]] float Sample2D(glm::vec2 pos) const override;

	/// 3D variant.
	[[nodiscard]] float Sample3D(glm::vec3 pos) const override;

	/// Returns {F1, F2} distances for this point (both computed in one pass).
	[[nodiscard]] glm::vec2 SampleCellular2D(glm::vec2 pos) const override;

	/// 3D variant.
	[[nodiscard]] glm::vec2 SampleCellular3D(glm::vec3 pos) const override;

private:
	WorkleyMode mode_;
	std::array<int, 512> perm_{};

	void BuildPermutationTable(int seed);

	/// Feature-point offset in [0, 1) for grid cell (cx, cy).
	[[nodiscard]] glm::vec2 FeaturePoint2D(int cx, int cy) const;

	/// Feature-point offset for 3D grid cell.
	[[nodiscard]] glm::vec3 FeaturePoint3D(int cx, int cy, int cz) const;

	/// Combine F1/F2 distances according to the configured mode.
	[[nodiscard]] float Combine(float f1, float f2) const;
};

} // namespace engine::noise::builtin
