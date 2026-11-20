#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include <flecs.h>

// The boss: when the Act reaches its Boss Stage the crabnado comes ashore and chases the player. Its bosses.json entry
// names the behavior. `slam` (Act 1): a big crest that pushes it into a rock is a slam, and enough slams beat it; the horde
// is swept, shells are vacuumed and it retreats to its orbit. `enraged` (Act 2): it chases, then winds up a volley or
// a suction, never the same twice running. A slam costs it HP and dazes it, and only while dazed do claw snaps and bubbles
// hurt it; 0 HP beats it. It is the same Crabnado entity; its orbit gives way to the Breach and then this Fight while
// RunState::phase is Boss (see crabnado.cpp).
namespace crabnado {

// What the enraged crabnado is doing. Chase: closing on the player until `timer`'s gap runs out. Windup: stopped, funnel
// strobing, the attack's telegraph showing. Active: the attack running (suction's pull). Recover: a beat after an attack.
// Dazed: stopped, harmless, and hurt by weapons.
enum class BossAction { Chase, Windup, Active, Recover, Dazed };

enum class BossAttack { None, Volley, Suction };

// Singleton, reflected: the enraged crabnado's live state, readable over the Remote API. `timer` counts down the current
// Windup, Active, Recover or Dazed; `gap` the Chase seconds left before the next attack. `last_attack` keeps the picker
// from repeating. `hp` and `max_hp` are its health; `position` and `radius` where it stands and how big it is, for the
// weapons that hit it. It is reset whenever the Boss phase isn't running.
struct BossState {
	BossAction action{BossAction::Chase};
	BossAttack attack{BossAttack::None};
	BossAttack last_attack{BossAttack::None};
	float timer{0.0F};
	float gap{0.0F};
	float hp{0.0F};
	float max_hp{0.0F};
	glm::vec2 position{0.0F};
	float radius{0.0F};
};

// A yellow disc on the water where a volley crab will land, a child of the horde. When that crab lands (or is gone) the
// disc goes, and a landing within `radius` of the player costs `damage` HP once.
struct VolleyDisc {
	uint64_t enemy{0};
	glm::vec2 landing{0.0F};
	float radius{0.0F};
	float damage{0.0F};
};

// Tag on the red ring that warns of a suction, a child of the crabnado.
struct BossRing {};

// The enraged crabnado's state when it is dazed and can be hurt, else nullptr.
[[nodiscard]] const BossState* DazedBoss(const flecs::world& world);

// Takes `amount` HP off the enraged crabnado; the BossUpdate system finishes it at 0. Callers check DazedBoss first.
void DamageBoss(const flecs::world& world, float amount);

// Registers and reflects the boss components and the BossUpdate and VolleyDiscLand systems. Call from the module
// constructor after RegisterCrabnado.
void RegisterBoss(const flecs::world& world);

} // namespace crabnado
