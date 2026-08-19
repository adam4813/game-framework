#include "particles_module.hpp"

#include <cstdint>

#include <spdlog/spdlog.h>

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/ecs/ecs.hpp"
#include "engine/render/render.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"
#include "particle_components.hpp"

namespace engine::particles {

namespace {

std::uint8_t LerpU8(const std::uint8_t a, const std::uint8_t b, const float t) {
	return static_cast<std::uint8_t>(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t);
}

// Linear interpolation between two colours, including the alpha channel (used for fade-out).
platform::Rgba LerpRgba(const platform::Rgba a, const platform::Rgba b, const float t) {
	return {LerpU8(a.r, b.r, t), LerpU8(a.g, b.g, t), LerpU8(a.b, b.b, t), LerpU8(a.a, b.a, t)};
}

} // namespace

ParticlesModule::ParticlesModule(const flecs::world& world) {
	// === Reflection ===
	world.component<ParticleEmitter>()
		.member<float>("rate")
		.member<float>("accumulator")
		.member<float>("particleLifetime")
		.member<float>("speed")
		.member<float>("startSize")
		.member<float>("spread")
		.member<glm::vec3>("direction")
		.member<platform::Rgba>("colorStart")
		.member<platform::Rgba>("colorEnd")
		.member<bool>("emitting");
	world.component<Particle>()
		.member<glm::vec3>("velocity")
		.member<float>("age")
		.member<float>("lifetime")
		.member<float>("startSize")
		.member<platform::Rgba>("colorStart")
		.member<platform::Rgba>("colorEnd");

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<ParticleEmitter>());
	scripting::RegisterComponentForScripts(world, world.component<Particle>());

	// === SYSTEM: ParticleEmit — spawn particles from each emitter over time ===
	world.system<ParticleEmitter, const ecs::WorldTransform>("ParticleEmit")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, ParticleEmitter& emitter, const ecs::WorldTransform& wt) {
			if (!emitter.emitting) {
				return;
			}
			emitter.accumulator += emitter.rate * it.delta_time();
			auto& rng = it.world().get_mut<ecs::RngState>();
			while (emitter.accumulator >= 1.0F) {
				emitter.accumulator -= 1.0F;

				const glm::vec3 jitter{
					(rng.NextFloat() * 2.0F - 1.0F) * emitter.spread,
					(rng.NextFloat() * 2.0F - 1.0F) * emitter.spread,
					(rng.NextFloat() * 2.0F - 1.0F) * emitter.spread
				};
				glm::vec3 dir = emitter.direction + jitter;
				if (glm::dot(dir, dir) < 1e-6F) {
					dir = emitter.direction;
				}
				const glm::vec3 velocity = glm::normalize(dir) * emitter.speed;

				const auto particle = it.world().entity().child_of(it.entity(i));
				particle.set<Particle>(
					{.velocity = velocity,
					 .age = 0.0F,
					 .lifetime = emitter.particleLifetime,
					 .startSize = emitter.startSize,
					 .colorStart = emitter.colorStart,
					 .colorEnd = emitter.colorEnd}
				);
				particle.set<render::SpherePrimitive>({.radius = emitter.startSize});
				particle.set<render::Material>({.color = emitter.colorStart, .cast_shadow = false});
				ecs::WorldTransform pwt{};
				pwt.position = wt.position;
				pwt.scale = glm::vec3{1.0F};
				pwt.ComputeMatrix();
				particle.set<ecs::WorldTransform>(pwt);
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// === SYSTEM: ParticleUpdate — advance motion + lifetime, fade, and cull ===
	world.system<Particle, ecs::WorldTransform, render::Material, render::SpherePrimitive>("ParticleUpdate")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it,
				 const size_t i,
				 Particle& particle,
				 ecs::WorldTransform& wt,
				 render::Material& mat,
				 render::SpherePrimitive& sphere) {
			particle.age += it.delta_time();
			if (particle.age >= particle.lifetime) {
				it.entity(i).destruct();
				return;
			}
			const float k = particle.lifetime > 0.0F ? particle.age / particle.lifetime : 1.0F;
			wt.position += particle.velocity * it.delta_time();
			sphere.radius = particle.startSize * (1.0F - k);
			mat.color = LerpRgba(particle.colorStart, particle.colorEnd, k);
			wt.ComputeMatrix();
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	spdlog::info("[ParticlesModule] Registered particle emit/update systems with Flecs");
}

} // namespace engine::particles
