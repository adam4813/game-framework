#include "horde.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "crabnado_constants.hpp"
#include "crowd_grid.hpp"
#include "player.hpp"
#include "run.hpp"
#include "shells.hpp"

using namespace engine;

namespace crabnado {

namespace {

// Crab look, from the prototype: a flattened body sphere with two claw spheres ahead of it (local +z
// is forward).
constexpr core::Rgba kCrabColor{.r = 216, .g = 69, .b = 43, .a = 255};
constexpr core::Rgba kFlashColor{.r = 255, .g = 255, .b = 255, .a = 255};
constexpr glm::vec3 kBodyOffset{0.0F, 0.45F, 0.0F};
constexpr glm::vec3 kBodyScale{1.0F, 0.5F, 0.75F};
constexpr float kClawRadius = 0.32F;
constexpr glm::vec3 kClawOffset{0.85F, 0.5F, 0.7F};
// Spin of a crab tumbling through the air, in radians per second.
constexpr float kFlightSpin = 8.0F;
// Landed enemies may stray this far past the arena edge before being held back.
constexpr float kEdgeSlack = 1.0F;
// Throws land at least this far inside the arena edge.
constexpr float kLandingMargin = 2.0F;
// Overlap below this fraction of the touching distance is tolerated, so a crowd settles instead of jittering.
constexpr float kSeparationSlack = 0.9F;
// A touching neighbour within acos(kBlockedCone) of an enemy's heading blocks its step (about ±60°).
constexpr float kBlockedCone = 0.5F;

void AddPart(const flecs::world& world, const flecs::entity crab, const glm::vec3 position, const glm::vec3 scale,
	const float radius) {
	std::ignore = world.entity()
					  .child_of(crab)
					  .set<spatial::Transform>({.position = position, .scale = scale})
					  .set<render::SpherePrimitive>({.radius = radius})
					  .set<render::Material>({.color = kCrabColor});
}

void SetTint(const flecs::entity enemy, const core::Rgba color) {
	enemy.children([color](const flecs::entity part) {
		if (part.has<render::Material>()) {
			part.get_mut<render::Material>().color = color;
		}
	});
}

glm::vec2 ClampToRadius(const glm::vec2 point, const float radius) {
	const float distance = glm::length(point);
	return distance > radius ? point * (radius / distance) : point;
}

} // namespace

void RegisterHorde(const flecs::world& world) {
	world.component<Enemy>()
		.member<float>("hp")
		.member<float>("speed")
		.member<float>("radius")
		.member<float>("contact_dps")
		.member<int>("xp")
		.member<float>("flash")
		.member<core::Rgba>("color");
	world.component<Airborne>().member<glm::vec3>("velocity");
	world.component<HordeReadout>().member<int>("enemy_count").member<int>("fps");
	world.set<HordeReadout>({});
	world.component<CrowdGrid>()
		.member("cells_per_side", &CrowdGrid::cells_per_side)
		.member("max_radius", &CrowdGrid::max_radius);
	CrowdGrid grid;
	grid.Resize(constants::kCrowdCellSize, constants::kArenaRadius + 2.0F * constants::kCrowdCellSize);
	world.set<CrowdGrid>(std::move(grid));

	// Ballistic flight; an enemy joins the horde when it touches the ground.
	world.system<Airborne, spatial::Transform>("HordeFlight")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, Airborne& air, spatial::Transform& transform) {
			const float dt = it.delta_time();
			air.velocity.y -= constants::kGravity * dt;
			transform.position += air.velocity * dt;
			transform.rotation.y += kFlightSpin * dt;
			if (transform.position.y <= 0.0F) {
				transform.position.y = 0.0F;
				it.entity(i).remove<Airborne>();
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	world.system<const Enemy, const spatial::Transform>("CrowdGridRebuild")
		.without<Airborne>()
		.kind(flecs::OnUpdate)
		.run([](flecs::iter& it) {
			auto& grid = it.world().get_mut<CrowdGrid>();
			grid.scratch.clear();
			while (it.next()) {
				const auto enemies = it.field<const Enemy>(0);
				const auto transforms = it.field<const spatial::Transform>(1);
				for (const auto i : it) {
					grid.scratch.push_back(
						{.entity = it.entity(i).id(),
						 .position = {transforms[i].position.x, transforms[i].position.z},
						 .radius = enemies[i].radius}
					);
				}
			}
			grid.Rebuild();
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// Landed enemies walk sideways toward the player and push apart where they overlap.
	world.system<const Enemy, spatial::Transform>("HordeChase")
		.without<Airborne>()
		.kind(flecs::OnUpdate)
		.run([](flecs::iter& it) {
			const flecs::entity player = FindPlayer(it.world());
			if (!player) {
				while (it.next()) {
				}
				return;
			}
			const glm::vec3 playerPosition = player.get<spatial::Transform>().position;
			const glm::vec2 target{playerPosition.x, playerPosition.z};
			const auto& grid = it.world().get<CrowdGrid>();
			const float dt = it.delta_time();
			while (it.next()) {
				const auto enemies = it.field<const Enemy>(0);
				auto transforms = it.field<spatial::Transform>(1);
				for (const auto i : it) {
					const Enemy& enemy = enemies[i];
					spatial::Transform& transform = transforms[i];
					const glm::vec2 position{transform.position.x, transform.position.z};
					const glm::vec2 toTarget = target - position;
					const float distance = glm::length(toTarget);
					const glm::vec2 heading = distance > 0.0001F ? toTarget / distance : glm::vec2{0.0F};

					glm::vec2 push{0.0F};
					bool blocked = false;
					const flecs::entity_t self = it.entity(i).id();
					grid.ForEachNear(position, enemy.radius + grid.max_radius, [&](const CrowdEntry& other) {
						const glm::vec2 apart = position - other.position;
						const float gap = glm::length(apart);
						const float touching = (enemy.radius + other.radius) * kSeparationSlack;
						if (other.entity != self && gap > 0.0001F && gap < touching) {
							push += apart * ((touching - gap) / gap);
							// A neighbour in the cone ahead blocks the way: queue behind it instead of pushing in.
							blocked = blocked || glm::dot(-apart, heading) > kBlockedCone * gap;
						}
					});

					const float step = blocked ? 0.0F : enemy.speed * dt;
					const glm::vec2 moved = ClampToRadius(
						position + heading * step + push * constants::kSeparationRelax,
						constants::kArenaRadius + kEdgeSlack
					);
					transform.position.x = moved.x;
					transform.position.z = moved.y;
					if (distance > 0.0001F) {
						// Crabs walk sideways: face 90° off the direction of travel.
						transform.rotation.y = std::atan2(heading.x, heading.y) + std::numbers::pi_v<float> / 2.0F;
					}
				}
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// Landed enemies touching the player drain its HP.
	world.system("HordeContactDamage")
		.kind(flecs::OnUpdate)
		.run([](flecs::iter& it) {
			const flecs::world world = it.world();
			const flecs::entity player = FindPlayer(world);
			if (!player) {
				return;
			}
			const glm::vec3 p = player.get<spatial::Transform>().position;
			const glm::vec2 position{p.x, p.z};
			const auto& grid = world.get<CrowdGrid>();
			float damage = 0.0F;
			grid.ForEachNear(position, grid.max_radius + constants::kPlayerContactRadius, [&](const CrowdEntry& e) {
				if (glm::length(e.position - position) < e.radius + constants::kPlayerContactRadius) {
					damage += world.entity(e.entity).get<Enemy>().contact_dps;
				}
			});
			if (damage > 0.0F) {
				float& hp = player.get_mut<PlayerStats>().hp;
				hp = std::max(0.0F, hp - damage * it.delta_time());
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	world.system<const Enemy, const spatial::Transform>("EnemyDefeat")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, const Enemy& enemy, const spatial::Transform& transform) {
			if (enemy.hp > 0.0F) {
				return;
			}
			const flecs::world world = it.world();
			const flecs::entity defeated = it.entity(i);
			++world.get_mut<RunState>().kills;
			DropShell(world, defeated.parent(), {transform.position.x, transform.position.z}, enemy.xp);
			defeated.destruct();
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	world.system<Enemy>("EnemyHitFlash")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, Enemy& enemy) {
			if (enemy.flash <= 0.0F) {
				return;
			}
			enemy.flash -= it.delta_time();
			if (enemy.flash <= 0.0F) {
				SetTint(it.entity(i), enemy.color);
			}
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

void ThrowCrab(const flecs::world& world, const flecs::entity parent, const glm::vec2 from, const glm::vec2 target) {
	auto& rng = world.get_mut<ecs::RngState>();
	const glm::vec2 toTarget = target - from;
	const float distance = glm::length(toTarget);
	const glm::vec2 heading = distance > 0.0001F ? toTarget / distance : glm::vec2{0.0F, 1.0F};
	const float reach = rng.NextFloatRange(constants::kThrowMinDistance, constants::kThrowMaxDistance);
	const glm::vec2 jitter{
		rng.NextFloatRange(-constants::kThrowJitter, constants::kThrowJitter),
		rng.NextFloatRange(-constants::kThrowJitter, constants::kThrowJitter)
	};
	const glm::vec2 landing =
		ClampToRadius(from + heading * reach + jitter, constants::kArenaRadius - kLandingMargin);

	// Launch speed for a flight time T from height y0 under gravity G (the prototype's formula).
	const float flightTime = rng.NextFloatRange(constants::kThrowMinTime, constants::kThrowMaxTime);
	const float y0 = constants::kThrowHeight;
	const glm::vec2 ground = (landing - from) / flightTime;
	const float vy = (0.5F * constants::kGravity * flightTime * flightTime - y0) / flightTime;

	const float clock = world.get<RunState>().clock;
	const flecs::entity crab =
		world.entity()
			.child_of(parent)
			.set<Enemy>(
				{.hp = constants::kCrabHp * (1.0F + clock / constants::kEnemyHpGrowthSeconds),
				 .speed = constants::kCrabSpeed,
				 .radius = constants::kCrabRadius,
				 .contact_dps = constants::kCrabContactDps,
				 .xp = constants::kCrabXp,
				 .color = kCrabColor}
			)
			.set<Airborne>({.velocity = {ground.x, vy, ground.y}})
			.set<spatial::Transform>({.position = {from.x, y0, from.y}});
	AddPart(world, crab, kBodyOffset, kBodyScale, 1.0F);
	AddPart(world, crab, kClawOffset * glm::vec3{-1.0F, 1.0F, 1.0F}, glm::vec3{1.0F}, kClawRadius);
	AddPart(world, crab, kClawOffset, glm::vec3{1.0F}, kClawRadius);
}

void DamageEnemy(const flecs::entity enemy, const float amount) {
	auto& state = enemy.get_mut<Enemy>();
	state.hp -= amount;
	state.flash = constants::kHitFlashTime;
	SetTint(enemy, kFlashColor);
}

} // namespace crabnado
