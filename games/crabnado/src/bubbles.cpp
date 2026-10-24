#include "bubbles.hpp"

#include <cmath>

#include "arena.hpp"
#include "crabnado.hpp"
#include "crabnado_constants.hpp"
#include "crowd_grid.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "sfx.hpp"
#include "waves.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr core::Rgba kBubbleColor{.r = 191, .g = 243, .b = 255, .a = 255};
constexpr float kBubbleHeight = 1.0F;

// The nearest landed enemy within kBubbleRange of `from`, by a linear scan. Returns false when there is none.
bool FindTarget(const flecs::world& world, const glm::vec2 from, glm::vec2& target) {
	float best = constants::kBubbleRange;
	bool found = false;
	for (const CrowdEntry& e : world.get<CrowdGrid>().entries) {
		const float distance = glm::length(e.position - from);
		if (distance < best) {
			best = distance;
			target = e.position;
			found = true;
		}
	}
	return found;
}

// The horde root the crabnado throws into, or an invalid entity when there is no crabnado.
flecs::entity FindHorde(const flecs::world& world) {
	flecs::entity horde;
	world.each([&](const flecs::entity crabnado, const Crabnado&) { horde = crabnado.target<ThrowsInto>(); });
	return horde;
}

} // namespace

void RegisterBubbles(const flecs::world& world) {
	world.component<Bubbles>()
		.member<float>("damage")
		.member<float>("cooldown")
		.member<int>("count")
		.member<float>("speed")
		.member<float>("timer");
	world.component<Bubble>().member<glm::vec2>("velocity").member<float>("life");

	world.system<Bubbles, const spatial::Transform>("BubbleFire")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, Bubbles& bubbles, const spatial::Transform& transform) {
			bubbles.timer = std::max(0.0F, bubbles.timer - it.delta_time());
			if (bubbles.timer > 0.0F) {
				return;
			}
			const flecs::world world = it.world();
			const glm::vec2 position{transform.position.x, transform.position.z};
			glm::vec2 target;
			const flecs::entity horde = FindHorde(world);
			if (!horde || !FindTarget(world, position, target)) {
				return;
			}
			bubbles.timer = bubbles.cooldown;
			const glm::vec2 toTarget = target - position;
			const float base = std::atan2(toTarget.x, toTarget.y);
			const int room = constants::kBubbleCap - world.count<Bubble>();
			for (int k = 0; k < std::min(bubbles.count, room); ++k) {
				const float angle = base + (static_cast<float>(k) - static_cast<float>(bubbles.count - 1) / 2.0F) * constants::kBubbleSpread;
				std::ignore = world.entity()
								  .child_of(horde)
								  .set<spatial::Transform>({.position = {position.x, kBubbleHeight, position.y}})
								  .set<Bubble>({.velocity = {std::sin(angle) * bubbles.speed, std::cos(angle) * bubbles.speed},
									  .life = constants::kBubbleLife})
								  .set<render::SpherePrimitive>({.radius = constants::kBubbleRadius})
								  .set<render::Material>({.color = kBubbleColor, .cast_shadow = false, .unlit = true});
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	world.system<Bubble, spatial::Transform>("BubbleFly")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, Bubble& bubble, spatial::Transform& transform) {
			const flecs::world world = it.world();
			const float dt = it.delta_time();
			glm::vec2 position{transform.position.x, transform.position.z};
			const WaveSample wave = SampleWaves(world.get<WaveField>(), world.get<TidePools>().pools, position);
			bubble.velocity += (WindPush(world.get<Wind>(), constants::kBubbleWindDrift) + wave.push * constants::kBubbleWaveDrift) * dt;
			position += bubble.velocity * dt;
			transform.position.x = position.x;
			transform.position.z = position.y;
			bubble.life -= dt;
			if (bubble.life <= 0.0F) {
				it.entity(i).destruct();
				return;
			}
			const auto& grid = world.get<CrowdGrid>();
			const flecs::entity player = FindPlayer(world);
			if (!player) {
				return;
			}
			const float damage = player.get<Bubbles>().damage;
			bool popped = false;
			grid.ForEachNear(position, constants::kBubbleRadius + grid.max_radius, [&](const CrowdEntry& e) {
				if (!popped && glm::length(e.position - position) < e.radius + constants::kBubbleRadius) {
					DamageEnemy(world.entity(e.entity), damage);
					popped = true;
				}
			});
			if (popped) {
				PlaySfx(world, Sfx::BubblePop);
				it.entity(i).destruct();
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

void AddBubbles(const flecs::entity player) {
	player.set<Bubbles>(
		{.damage = constants::kBubbleDamage,
		 .cooldown = constants::kBubbleCooldown,
		 .count = 1,
		 .speed = constants::kBubbleSpeed,
		 .timer = 0.0F}
	);
}

} // namespace crabnado
