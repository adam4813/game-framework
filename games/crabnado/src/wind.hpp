#pragma once

#include <glm/glm.hpp>

#include <flecs.h>

// The wind: weather that shifts direction every 20–30 s with a warning, gusts now and then, and pushes
// the player, the horde and the crabnado's roaming slightly downwind. Its state is one reflected
// singleton, so the Remote API can read and set it. Wind streaks and a north-up HUD compass show it.
namespace crabnado {

// Singleton: the current wind. `direction` is the compass bearing the wind blows toward, in radians
// clockwise from north (0 blows toward north, world -z; pi/2 toward east, world +x). `turn` is how much
// of the current shift is still to turn, signed, so setting `direction` over the Remote API keeps any turn
// relative to it. `strength` is about 1 (0.6 calm to 1.5 in a gust); `gust` is the gust's share of it.
// `warning` is set for a few seconds before each shift. `shifts` counts the shifts so far.
struct Wind {
	float direction{0.0F};
	float strength{1.0F};
	float gust{0.0F};
	float turn{0.0F};
	float shift_timer{0.0F};
	float gust_timer{0.0F};
	float time{0.0F};
	bool warning{false};
	int shifts{0};
};

// A wind streak: a short line that rides the wind around the player. `pace` scales its speed and length.
struct WindStreak {
	float pace{1.0F};
};

// Where the arena HUD draws the wind compass, in screen pixels.
struct WindCompass {
	glm::vec2 center{0.0F};
	float radius{0.0F};
};

// Registers and reflects the wind components and the Wind singleton, and the WindBlow, WindStreakMove and
// WindCompassDraw systems. Call from the module constructor before the systems that read the wind.
void RegisterWind(const flecs::world& world);

// Starts a fresh wind for a run: a random direction and shift and gust timers, drawn from RngState.
void StartWind(const flecs::world& world);

// The unit vector the wind blows along on the ground plane (x, z).
[[nodiscard]] glm::vec2 WindDirection(const Wind& wind);

// The wind's push on the ground plane: WindDirection * strength * `scale`, in units per second.
[[nodiscard]] glm::vec2 WindPush(const Wind& wind, float scale);

// The wind's strength as a Beaufort-style force number for the HUD.
[[nodiscard]] int WindForce(const Wind& wind);

// Spawns the wind streaks around the arena centre as children of `parent`.
void SpawnWindStreaks(const flecs::world& world, flecs::entity parent);

} // namespace crabnado
