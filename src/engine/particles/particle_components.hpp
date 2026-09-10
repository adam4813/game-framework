#pragma once

#include <glm/glm.hpp>

#include "engine/core/core.hpp"

// Particle/VFX components. Particles are ordinary ECS entities that reuse the existing render
// pipeline: each spawned particle carries a spatial::Transform + render::SpherePrimitive +
// render::Material, so the render systems draw them with no new draw code. This module only spawns,
// simulates, fades and culls them.
namespace engine::particles {

// Emitter attached to an entity. Spawns `rate` particles per second while `emitting`, each launched
// along `direction` (in the emitter's local space) with a random cone of half-width `spread`, fading
// from `colorStart` to `colorEnd` over `particleLifetime`. Particles are parented to the emitter, so
// moving the emitter (or its parent) carries them.
struct ParticleEmitter {
	float rate{20.0F};
	float accumulator{0.0F}; // fractional spawn debt carried between frames
	float particleLifetime{1.0F};
	float speed{2.0F};
	float startSize{0.15F};
	float spread{0.5F};
	glm::vec3 direction{0.0F, 1.0F, 0.0F};
	core::Rgba colorStart{255, 255, 255, 255};
	core::Rgba colorEnd{255, 255, 255, 0};
	bool emitting{true};
};

// One live particle. Its position lives in the entity's local spatial::Transform (advanced by ParticleMove
// and composed by TransformPropagation); this holds the per-particle simulation state the systems read.
struct Particle {
	glm::vec3 velocity{0.0F};
	float age{0.0F};
	float lifetime{1.0F};
	float startSize{0.15F};
	core::Rgba colorStart{255, 255, 255, 255};
	core::Rgba colorEnd{255, 255, 255, 0};
};

} // namespace engine::particles
