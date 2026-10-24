#include "shells.hpp"

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "player.hpp"
#include "run.hpp"
#include "waves.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr core::Rgba kShellColor{.r = 255, .g = 244, .b = 220, .a = 255};
constexpr float kShellRadius = 0.35F;
constexpr float kShellHeight = 0.6F;
constexpr glm::vec3 kShellScale{1.0F, 0.6F, 1.0F};

} // namespace

void RegisterShells(const flecs::world& world) {
	world.component<Shell>().member<int>("xp").member<bool>("flying");

	world.system<Shell, spatial::Transform>("ShellPickup")
		.kind(flecs::OnUpdate)
		.run([](flecs::iter& it) {
			const flecs::world world = it.world();
			const flecs::entity player = FindPlayer(world);
			if (!player) {
				while (it.next()) {
				}
				return;
			}
			const glm::vec3 p = player.get<spatial::Transform>().position;
			const glm::vec2 target{p.x, p.z};
			const float range = player.get<PlayerStats>().pickup_range;
			const float dt = it.delta_time();
			const auto& waves = world.get<WaveField>();
			int collected = 0;
			while (it.next()) {
				auto shells = it.field<Shell>(0);
				auto transforms = it.field<spatial::Transform>(1);
				for (const auto i : it) {
					Shell& shell = shells[i];
					spatial::Transform& transform = transforms[i];
					const glm::vec2 position{transform.position.x, transform.position.z};
					const glm::vec2 toPlayer = target - position;
					const float distance = glm::length(toPlayer);
					shell.flying = shell.flying || distance < range;
					if (!shell.flying) {
						// Unclaimed shells ride the water and drift with the waves.
						const WaveSample wave = SampleWaves(waves, {}, position);
						const glm::vec2 drifted = position + wave.push * constants::kShellWavePush * dt;
						transform.position = {drifted.x, wave.height * constants::kWaterHeightScale + kShellHeight, drifted.y};
						continue;
					}
					if (distance < constants::kShellCollectRadius) {
						collected += shell.xp;
						it.entity(i).destruct();
						continue;
					}
					const glm::vec2 moved = position + toPlayer / distance * constants::kShellFlySpeed * dt;
					transform.position.x = moved.x;
					transform.position.z = moved.y;
				}
			}
			if (collected > 0) {
				world.get_mut<RunState>().shells += collected;
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

void DropShell(const flecs::world& world, const flecs::entity parent, const glm::vec2 position, const int xp) {
	std::ignore = world.entity()
					  .child_of(parent)
					  .set<Shell>({.xp = xp})
					  .set<spatial::Transform>({.position = {position.x, kShellHeight, position.y}, .scale = kShellScale})
					  .set<render::SpherePrimitive>({.radius = kShellRadius})
					  .set<render::Material>({.color = kShellColor, .cast_shadow = false, .unlit = true});
}

} // namespace crabnado
