# Particle System

Lightweight CPU particles that reuse the existing render pipeline. A particle is an ordinary ECS entity carrying
`ecs::WorldTransform` + `render::SpherePrimitive` + `render::Material`, so the render systems draw it with no new draw
code. This module only spawns, simulates, fades and culls particles.

## Structure

- **particle_components.hpp** — `ParticleEmitter` and `Particle` plain-data components.
- **particles_module.hpp/cpp** — `ParticlesModule` registering the emit + update systems.
- **particles.hpp** — public umbrella header.

## Usage

Attach a `ParticleEmitter` to an entity that also has an `ecs::WorldTransform` (the spawn origin):

```cpp
auto emitter = world.entity("HitBurst");
emitter.set<engine::ecs::WorldTransform>(originWorldTransform);
emitter.set<engine::particles::ParticleEmitter>({
    .rate = 40.0F,
    .particleLifetime = 0.6F,
    .speed = 3.0F,
    .startSize = 0.1F,
    .spread = 0.7F,
    .direction = {0.0F, 1.0F, 0.0F},
    .colorStart = {255, 210, 90, 255},
    .colorEnd = {255, 80, 40, 0},
});
```

Set `emitting = false` to pause emission (existing particles finish their lifetime).

## Registered systems

- **ParticleEmit** — per-emitter `each()` in `flecs::OnUpdate` (Pausable, GameScene-scoped). Spawns child particle
  entities over time using the emitter's rate/direction/spread and the world
  `RngState`.
- **ParticleUpdate** — per-particle `each()` in `flecs::OnUpdate` (Pausable, GameScene-scoped). Advances position,
  shrinks the sphere, lerps the material colour/alpha toward `colorEnd`, and destroys the particle when its lifetime
  elapses.
