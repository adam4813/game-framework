#include "particles_module.hpp"

#include <cstdint>

#include <flecs.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/core/core.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"
#include "engine/render/render.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scripting/scripting.hpp"
#include "engine/spatial/spatial.hpp"
#include "particle_components.hpp"

namespace engine::particles {

namespace {

std::uint8_t LerpU8(const std::uint8_t a, const std::uint8_t b, const float t) {
	return static_cast<std::uint8_t>(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t);
}

// Linear interpolation between two colours, including the alpha channel (used for fade-out).
core::Rgba LerpRgba(const core::Rgba a, const core::Rgba b, const float t) {
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
		.member<core::Rgba>("colorStart")
		.member<core::Rgba>("colorEnd")
		.member<bool>("emitting");
	world.component<Particle>()
		.member<glm::vec3>("velocity")
		.member<float>("age")
		.member<float>("lifetime")
		.member<float>("startSize")
		.member<core::Rgba>("colorStart")
		.member<core::Rgba>("colorEnd");

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<ParticleEmitter>());
	scripting::RegisterComponentForScripts(world, world.component<Particle>());

	// === Level loader ===
	level::RegisterComponentLoader(world, "particle_emitter", [](const flecs::entity e, const nlohmann::json& j) {
		ParticleEmitter em{};
		em.rate = j.value("rate", em.rate);
		em.particleLifetime = j.value("lifetime", em.particleLifetime);
		em.speed = j.value("speed", em.speed);
		em.startSize = j.value("start_size", em.startSize);
		em.spread = j.value("spread", em.spread);
		em.direction = core::JVec3(j, "direction", em.direction);
		em.colorStart = core::JRgba(j, "color_start", em.colorStart);
		em.colorEnd = core::JRgba(j, "color_end", em.colorEnd);
		em.emitting = j.value("emitting", em.emitting);
		e.set<ParticleEmitter>(em);
	});

	// === SYSTEM: ParticleEmit — spawn particles from each emitter over time ===
	std::ignore =
		world.system<ParticleEmitter>("ParticleEmit")
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter& it, const size_t i, ParticleEmitter& emitter) {
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

					// Spawn as a child of the emitter with a *local* Transform starting at the emitter
					// origin. ParticleMove integrates velocity into that local Transform and
					// TransformPropagation composes it with the emitter's transform — so particles
					// follow a moving/parented emitter instead of being pinned to a world-space snapshot.
					const auto particle = it.world()
											  .entity()
											  .child_of(it.entity(i))
											  .set<Particle>(
												  {.velocity = velocity,
												   .age = 0.0F,
												   .lifetime = emitter.particleLifetime,
												   .startSize = emitter.startSize,
												   .colorStart = emitter.colorStart,
												   .colorEnd = emitter.colorEnd}
											  )
											  .set<render::SpherePrimitive>({.radius = emitter.startSize})
											  .set<spatial::Transform>({});

					// Material entity is a child of the particle so it is destroyed automatically
					// when the particle dies — same RenderWith pattern used by all scene objects.
					particle.add<render::RenderWith>(it.world().entity().child_of(particle).set<render::Material>(
						{.color = emitter.colorStart, .cast_shadow = false}
					));
				}
			})
			.add<ecs::Pausable>()
			.add<scene::GameScene>();

	// === SYSTEM: ParticleMove — integrate velocity into the particle's local Transform ===
	// A dedicated movement pass (kept separate from lifetime/fade below) that updates spatial::Transform
	// and lets TransformPropagation recompute the world matrix — no direct world-transform writes.
	std::ignore =
		world.system<spatial::Transform, const Particle>("ParticleMove")
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter& it, const size_t, spatial::Transform& transform, const Particle& particle) {
				transform.position += particle.velocity * it.delta_time();
			})
			.add<ecs::Pausable>()
			.add<scene::GameScene>();

	// === SYSTEM: ParticleUpdate — advance lifetime, fade colour + size, and cull ===
	std::ignore =
		world.system<Particle, render::SpherePrimitive>("ParticleUpdate")
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter& it, const size_t i, Particle& particle, render::SpherePrimitive& sphere) {
				particle.age += it.delta_time();
				if (particle.age >= particle.lifetime) {
					it.entity(i).destruct();
					return;
				}
				const float k = particle.lifetime > 0.0F ? particle.age / particle.lifetime : 1.0F;
				sphere.radius = particle.startSize * (1.0F - k);
				// Fade colour via the particle's material entity (same RenderWith pattern as render systems).
				if (const auto mat_e = it.entity(i).target<render::RenderWith>(); mat_e.is_valid()) {
					mat_e.get_mut<render::Material>().color = LerpRgba(particle.colorStart, particle.colorEnd, k);
				}
			})
			.add<ecs::Pausable>()
			.add<scene::GameScene>();

	spdlog::info("[ParticlesModule] Registered particle emit/update systems with Flecs");
}

} // namespace engine::particles
