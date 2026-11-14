#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

// ECS components for ambient wandering creatures (ticket 10). Creatures are non-interactive: they
// carry no Interactable/DialogTrigger, only a wander behaviour and a dynamic upright sprite quad.
namespace game::farm {

// Tag marking an entity as an ambient creature (used to query/attach sprites and for filtering).
struct CreatureTag {};

// The creature-catalog kind this entity was spawned from ("cow", "deer", …). Resolves to a sprite
// and height via the CreatureCatalog at sprite-attach time.
struct CreatureKind {
	std::string kind;
};

// Per-creature wander state driven by the "creature_wander" behaviour-tree action. The creature
// ambles around `home` within `radius` tiles, taking one step every `moveInterval` seconds. `rng`
// is a self-contained per-creature RNG stream so wandering needs no shared singleton.
struct CreatureWander {
	glm::ivec2 home{0, 0};      // tile the creature was spawned on; it ambles around this
	int radius{3};              // max Chebyshev tile distance it strays from home
	float moveInterval{1.2f};   // seconds between amble steps
	float timer{0.0f};          // countdown to the next step
	std::uint32_t rng{0};       // per-creature wander RNG state (seeded at spawn)
};

} // namespace game::farm
