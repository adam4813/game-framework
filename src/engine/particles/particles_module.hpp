#pragma once

#include <flecs.h>

namespace engine::particles {

// Particles module for Flecs ECS integration.
// Registers the ParticleEmitter/Particle components and the emit + update systems. Particles are
// spawned as child entities that reuse render::SpherePrimitive + render::Material, so no new
// rendering code is needed. Systems are Pausable and scene-scoped to GameScene.
class ParticlesModule {
public:
	explicit ParticlesModule(const flecs::world& world);
};

} // namespace engine::particles
