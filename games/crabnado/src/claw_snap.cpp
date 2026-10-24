#include "claw_snap.hpp"

#include <cmath>
#include <numbers>

#include "crabnado_constants.hpp"
#include "crowd_grid.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"

using namespace engine;

namespace crabnado {

namespace {

// The ring is beads on a unit circle; scaling the ring entity by the claw radius spreads them to the reach.
constexpr int kRingBeads = 36;
constexpr float kRingBeadRadius = 0.12F;
constexpr float kRingHeight = 0.15F;
constexpr core::Rgba kRingColor{.r = 255, .g = 250, .b = 220, .a = 255};
// The ring grows from this fraction of the radius to the full radius while it shows.
constexpr float kRingStartScale = 0.6F;

} // namespace

void RegisterClawSnap(const flecs::world& world) {
	world.component<ClawSnap>()
		.member<float>("damage")
		.member<float>("radius")
		.member<float>("cooldown")
		.member<float>("timer")
		.member<float>("ring_timer");
	world.component<ClawRing>();

	world.system<ClawSnap, const spatial::Transform>("ClawSnapFire")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, ClawSnap& claw, const spatial::Transform& transform) {
			const float dt = it.delta_time();
			claw.ring_timer = std::max(0.0F, claw.ring_timer - dt);
			claw.timer -= dt;
			if (claw.timer > 0.0F) {
				return;
			}
			claw.timer += claw.cooldown;
			claw.ring_timer = constants::kClawRingTime;

			const flecs::world world = it.world();
			const glm::vec2 position{transform.position.x, transform.position.z};
			const auto& grid = world.get<CrowdGrid>();
			grid.ForEachNear(position, claw.radius + grid.max_radius, [&](const CrowdEntry& e) {
				if (glm::length(e.position - position) < claw.radius + e.radius) {
					DamageEnemy(world.entity(e.entity), claw.damage);
				}
			});
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	world.system<spatial::Transform>("ClawRingShow")
		.with<ClawRing>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, spatial::Transform& transform) {
			const ClawSnap& claw = it.entity(i).parent().get<ClawSnap>();
			float scale = 0.0F;
			if (claw.ring_timer > 0.0F) {
				const float shown = 1.0F - claw.ring_timer / constants::kClawRingTime;
				scale = claw.radius * (kRingStartScale + (1.0F - kRingStartScale) * shown);
			}
			transform.scale = {scale, 1.0F, scale};
		})
		.add<scene::GameScene>();
}

void AddClawSnap(const flecs::world& world, const flecs::entity player) {
	player.set<ClawSnap>(
		{.damage = constants::kClawDamage,
		 .radius = constants::kClawRadius,
		 .cooldown = constants::kClawCooldown,
		 .timer = constants::kClawCooldown}
	);
	const flecs::entity ring = world.entity("ClawRing")
								   .child_of(player)
								   .add<ClawRing>()
								   .set<spatial::Transform>({.position = {0.0F, kRingHeight, 0.0F}, .scale = {0.0F, 1.0F, 0.0F}});
	for (int i = 0; i < kRingBeads; ++i) {
		const float angle = 2.0F * std::numbers::pi_v<float> * static_cast<float>(i) / kRingBeads;
		std::ignore = world.entity()
						  .child_of(ring)
						  .set<spatial::Transform>({.position = {std::cos(angle), 0.0F, std::sin(angle)}})
						  .set<render::SpherePrimitive>({.radius = kRingBeadRadius})
						  .set<render::Material>({.color = kRingColor, .cast_shadow = false, .unlit = true});
	}
}

} // namespace crabnado
