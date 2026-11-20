#pragma once

#include <optional>

#include <flecs.h>

#include <glm/glm.hpp>

#include "waves.hpp"

// The `infested_crest` spawner (Act 3's seahorses): a Stage's (or the Boss Stage's) spawn table may carry an
// `infested_crest` entry. When a
// new crest of the primary swell is kInfestedWarning seconds from the arena's upwind barrier, it is infested with the
// entry's `chance`, at most one at a time and `min_gap` seconds apart. The water under it tints sickly green and the HUD
// warns; the riders (the entry's seahorses) spawn on the crest ~kInfestedSpawnReach units outside the barrier and surf in.
namespace crabnado {

// Singleton, reflected: the spawner's progress. `tracking` is set once a primary swell has been seen (`swell_t0` identifies
// it, `last_n` the newest crest already rolled for). `last_infest` is the wave-clock time of the last infested crest.
// While `active`, crest (`crest_swell`, `crest_n`) is infested; `riders_out` once its seahorses have spawned, of
// `rider_type` (an EnemyRegistry index), `riders` of them. `stamp` is the wave clock last seen: a clock that jumps back
// (a new run) resets the state. `captain` marks the crest carrying Captain Crab in (InfestForCaptain).
struct InfestState {
	bool tracking{false};
	float swell_t0{0.0F};
	int last_n{0};
	float last_infest{-1.0e9F};
	bool active{false};
	int crest_swell{0};
	int crest_n{0};
	bool riders_out{false};
	int rider_type{0};
	int riders{0};
	float stamp{0.0F};
	bool captain{false};
};

// Infests the next crest of the primary swell that hasn't yet reached the riders' spawn point, to carry Captain Crab in
// with `riders` of EnemyRegistry type `riderType` (replacing any infested crest in flight). False with no swell.
bool InfestForCaptain(const flecs::world& world, int riderType, int riders);

// Seconds until the infested crest reaches the arena's upwind barrier, while it hasn't yet; nullopt otherwise.
[[nodiscard]] std::optional<float> InfestedCrestIn(const flecs::world& world);

// How strongly the infested crest tints the water at `position`, 0-1: 0 with no infested crest or away from its front.
[[nodiscard]] float InfestedTint(const InfestState& state, const WaveField& field, glm::vec2 position);

// Registers and reflects InfestState and adds the InfestedCrest system. Call from the module constructor after RegisterHorde.
void RegisterInfested(const flecs::world& world);

} // namespace crabnado
