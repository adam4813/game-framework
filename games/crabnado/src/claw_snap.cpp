#include "claw_snap.hpp"

#include <cmath>
#include <numbers>

#include "boss.hpp"
#include "crabnado_constants.hpp"
#include "crowd_grid.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"
#include "sfx.hpp"

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
		.member<float>("level");
	world.component<ClawRing>();

	world.system<ClawSnap, const spatial::Transform>("ClawSnapFire")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, ClawSnap& claw, const spatial::Transform& transform) {
			const flecs::entity player = it.entity(i);
			auto& cooldown = player.lookup(kClawCooldownName).get_mut<timer::Cooldown>();
			if (!cooldown.Ready()) {
				return;
			}
			cooldown.duration = claw.cooldown;
			cooldown.remaining = claw.cooldown;
			auto& ring = player.lookup(kClawRingName).get_mut<timer::Cooldown>();
			ring.remaining = ring.duration;

			const flecs::world world = it.world();
			const glm::vec2 position{transform.position.x, transform.position.z};
			const auto& grid = world.get<CrowdGrid>();
			bool hit = false;
			grid.ForEachNear(position, claw.radius + grid.max_radius, [&](const CrowdEntry& e) {
				if (glm::length(e.position - position) < claw.radius + e.radius) {
					DamageEnemy(world.entity(e.entity), claw.damage);
					hit = true;
				}
			});
			// Only a dazed boss feels the claw.
			if (const BossState* boss = DazedBoss(world);
				boss != nullptr && glm::length(boss->position - position) < claw.radius + boss->radius) {
				DamageBoss(world, claw.damage);
				hit = true;
			}
			if (hit) {
				PlaySfx(world, Sfx::ClawSnap);
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	world.system<spatial::Transform, const timer::Cooldown>("ClawRingShow")
		.with<ClawRing>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, spatial::Transform& transform, const timer::Cooldown& ring) {
			const ClawSnap& claw = it.entity(i).parent().get<ClawSnap>();
			float scale = 0.0F;
			if (!ring.Ready()) {
				const float shown = 1.0F - ring.remaining / ring.duration;
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
		 .cooldown = constants::kClawCooldown}
	);
	// The first snap comes one full cooldown in.
	std::ignore = world.entity()
					  .child_of(player)
					  .set_name(kClawCooldownName)
					  .set<timer::Cooldown>({.remaining = constants::kClawCooldown, .duration = constants::kClawCooldown});
	const flecs::entity ring = world.entity()
								   .child_of(player)
								   .set_name(kClawRingName)
								   .add<ClawRing>()
								   .set<timer::Cooldown>({.remaining = 0.0F, .duration = constants::kClawRingTime})
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
