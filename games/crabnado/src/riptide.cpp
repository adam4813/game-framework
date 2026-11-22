#include "riptide.hpp"

#include <algorithm>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "waves.hpp"

using namespace engine;

namespace crabnado {

void RegisterRiptide(const flecs::world& world) {
	world.component<Riptide>()
		.member<float>("level")
		.member<float>("cooldown")
		.member<float>("multiplier");

	world.system<Riptide, const spatial::Transform>("RiptideFire")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, Riptide& riptide, const spatial::Transform& transform) {
			auto& cooldown = it.entity(i).lookup(kRiptideCooldownName).get_mut<timer::Cooldown>();
			if (riptide.level <= 0.0F) {
				// Locked: hold the first-fire delay so the unlock starts the countdown.
				cooldown.remaining = constants::kRiptideFirstDelay;
				return;
			}
			if (!cooldown.Ready()) {
				return;
			}
			auto& field = it.world().get_mut<WaveField>();
			const glm::vec2 position{transform.position.x, transform.position.z};
			if (BoostNextCrest(field, position, constants::kRiptideLead, riptide.multiplier)) {
				cooldown.duration = riptide.cooldown;
				cooldown.remaining = riptide.cooldown;
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

void AddRiptide(const flecs::world& world, const flecs::entity player) {
	player.set<Riptide>(
		{.level = 0.0F,
		 .cooldown = constants::kRiptideCooldown,
		 .multiplier = constants::kRiptideMultiplier}
	);
	std::ignore = world.entity()
					  .child_of(player)
					  .set_name(kRiptideCooldownName)
					  .set<timer::Cooldown>({.remaining = constants::kRiptideFirstDelay, .duration = constants::kRiptideCooldown});
}

} // namespace crabnado
