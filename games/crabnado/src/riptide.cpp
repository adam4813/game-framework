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
		.member<float>("multiplier")
		.member<float>("timer");

	world.system<Riptide, const spatial::Transform>("RiptideFire")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, Riptide& riptide, const spatial::Transform& transform) {
			if (riptide.level <= 0.0F) {
				return;
			}
			riptide.timer = std::max(0.0F, riptide.timer - it.delta_time());
			if (riptide.timer > 0.0F) {
				return;
			}
			auto& field = it.world().get_mut<WaveField>();
			const glm::vec2 position{transform.position.x, transform.position.z};
			if (BoostNextCrest(field, position, constants::kRiptideLead, riptide.multiplier)) {
				riptide.timer = riptide.cooldown;
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

void AddRiptide(const flecs::entity player) {
	player.set<Riptide>(
		{.level = 0.0F,
		 .cooldown = constants::kRiptideCooldown,
		 .multiplier = constants::kRiptideMultiplier,
		 .timer = constants::kRiptideFirstDelay}
	);
}

} // namespace crabnado
