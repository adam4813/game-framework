#pragma once

#include <glm/glm.hpp>

#include "engine/platform/platform.hpp"

// Particle/VFX components. Particles are ordinary ECS entities that reuse the existing render
// pipeline: each spawned particle carries an ecs::WorldTransform + render::SpherePrimitive +
// render::Material, so the render systems draw them with no new draw code. This module only spawns,
// simulates, fades and culls them.
namespace engine::particles {

// Emitter attached to an entity (which must also have an ecs::WorldTransform for its origin).
// Spawns `rate` particles per second while `emitting`, each launched along `direction` with a
// random cone of half-width `spread`, fading from `colorStart` to `colorEnd` over `particleLifetime`.
struct ParticleEmitter {
	float rate{20.0F};
	float accumulator{0.0F}; // fractional spawn debt carried between frames
	float particleLifetime{1.0F};
	float speed{2.0F};
	float startSize{0.15F};
	float spread{0.5F};
	glm::vec3 direction{0.0F, 1.0F, 0.0F};
	platform::Rgba colorStart{255, 255, 255, 255};
	platform::Rgba colorEnd{255, 255, 255, 0};
	bool emitting{true};
};

// One live particle. Its position/scale live in the entity's ecs::WorldTransform; this holds the
// per-particle simulation state the update system reads.
struct Particle {
	glm::vec3 velocity{0.0F};
	float age{0.0F};
	float lifetime{1.0F};
	float startSize{0.15F};
	platform::Rgba colorStart{255, 255, 255, 255};
	platform::Rgba colorEnd{255, 255, 255, 0};
};

} // namespace engine::particles
