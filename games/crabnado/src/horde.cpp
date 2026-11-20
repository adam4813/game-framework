#include "horde.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "arena.hpp"
#include "crabnado_constants.hpp"
#include "crowd_grid.hpp"
#include "enemy_types.hpp"
#include "player.hpp"
#include "run.hpp"
#include "sfx.hpp"
#include "shells.hpp"
#include "waves.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

namespace {

// Crab look, from the prototype: a flattened body sphere with two claw spheres ahead of it (local +z
// is forward).
constexpr core::Rgba kFlashColor{.r = 255, .g = 255, .b = 255, .a = 255};
// The wind-up and charge tells.
constexpr core::Rgba kWindupColor{.r = 255, .g = 200, .b = 40, .a = 255};
constexpr core::Rgba kChargeColor{.r = 255, .g = 40, .b = 90, .a = 255};
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
// The landing shadow: a disc of kShadowRadiusK times the crab's scale, squashed to kShadowFlatness, a little above the
// water. It starts at kShadowStartScale of its full size.
constexpr core::Rgba kShadowColor{.r = 10, .g = 20, .b = 30, .a = 140};
constexpr float kShadowRadiusK = 1.4F;
constexpr float kShadowStartScale = 0.5F;
constexpr float kShadowFlatness = 0.03F;
constexpr float kShadowY = 0.15F;
// Overlap below this fraction of the touching distance is tolerated, so a crowd settles instead of jittering.
constexpr float kSeparationSlack = 0.9F;
// A touching neighbour within acos(kBlockedCone) of an enemy's heading blocks its step (about ±60°).
constexpr float kBlockedCone = 0.5F;

void AddPart(const flecs::world& world, const flecs::entity enemy, const glm::vec3 position, const glm::vec3 scale,
	const float radius, const core::Rgba color) {
	std::ignore = world.entity()
					  .child_of(enemy)
					  .set<spatial::Transform>({.position = position, .scale = scale})
					  .set<render::SpherePrimitive>({.radius = radius})
					  .set<render::Material>({.color = color});
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
		.member<int>("type")
		.member<float>("tumble_min")
		.member<bool>("walks_sideways")
		.member<float>("flash")
		.member<core::Rgba>("color")
		.member<float>("tumble")
		.member<glm::vec2>("knock")
		.member<bool>("slammed")
		.member<float>("ride_age")
		.member<float>("ride_peak")
		.member<float>("immune")
		.member<float>("windup")
		.member<float>("charge")
		.member<glm::vec2>("charge_dir");
	world.component<Airborne>().member<glm::vec3>("velocity").member<glm::vec2>("landing");
	world.component<LandingShadow>().member<uint64_t>("enemy").member<float>("age").member<float>("duration");
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

	// A shadow grows with its enemy's flight and goes when the enemy lands or is gone (a swept horde, a Retry).
	world.system<LandingShadow, spatial::Transform>("LandingShadowGrow")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, LandingShadow& shadow, spatial::Transform& transform) {
			const flecs::world world = it.world();
			if (!world.is_alive(shadow.enemy) || !world.entity(shadow.enemy).has<Airborne>()) {
				it.entity(i).destruct();
				return;
			}
			shadow.age += it.delta_time();
			const float grown = kShadowStartScale + (1.0F - kShadowStartScale) * std::min(1.0F, shadow.age / shadow.duration);
			transform.scale = {grown, kShadowFlatness, grown};
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

	// Landed enemies walk sideways toward the player, drift downwind and push apart where they overlap. A crest
	// above an enemy's tumble threshold grabs it: it stops chasing and queueing and rides the push instead, then
	// winds up and charges the player.
	// They all ride the water surface.
	world.system<Enemy, spatial::Transform>("HordeChase")
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
			const glm::vec2 drift = WindPush(it.world().get<Wind>(), constants::kEnemyWindPush) * dt;
			const auto& waves = it.world().get<WaveField>();
			const auto& pools = it.world().get<TidePools>().pools;
			const auto& rocks = it.world().get<Rocks>().circles;
			const auto& types = it.world().get<EnemyRegistry>().types;
			while (it.next()) {
				auto enemies = it.field<Enemy>(0);
				auto transforms = it.field<spatial::Transform>(1);
				for (const auto i : it) {
					Enemy& enemy = enemies[i];
					spatial::Transform& transform = transforms[i];
					const glm::vec2 position{transform.position.x, transform.position.z};

					const EnemyType& type = types[static_cast<size_t>(enemy.type)];
					const flecs::entity self = it.entity(i);
					const auto endRide = [&] {
						enemy.tumble = 0.0F;
						enemy.ride_age = 0.0F;
						enemy.immune = type.immune;
						if (type.charges) {
							enemy.windup = std::max(0.001F, type.windup);
							SetTint(self, kWindupColor);
						}
					};

					// Ride, then wind up, then charge, then chase again. A ride never restarts: a taller crest
					// only raises its peak, and a finished ride makes the enemy immune to grabs for a while.
					const WaveSample wave = SampleWaves(waves, pools, position);
					const bool onCrest = wave.height > enemy.tumble_min;
					const float decay = std::exp(-constants::kKnockDecay * dt);
					enemy.immune = std::max(0.0F, enemy.immune - dt);
					if (enemy.tumble > 0.0F) {
						enemy.ride_age += dt;
						if (onCrest) {
							enemy.ride_peak = std::max(enemy.ride_peak, wave.height);
						}
						enemy.knock = onCrest ? wave.push * type.ride_push : enemy.knock * decay;
						enemy.tumble = RideDuration(type, enemy.ride_peak) - enemy.ride_age;
						if (enemy.tumble <= 0.0F) {
							endRide();
						}
					} else if (onCrest && enemy.immune <= 0.0F) {
						enemy.ride_age = 0.0F;
						enemy.ride_peak = wave.height;
						enemy.tumble = RideDuration(type, wave.height);
						enemy.knock = wave.push * type.ride_push;
						enemy.windup = 0.0F;
						enemy.charge = 0.0F;
						SetTint(self, enemy.color);
					} else {
						enemy.knock = onCrest ? wave.push * constants::kImmunePush : enemy.knock * decay;
					}
					if (enemy.windup > 0.0F) {
						enemy.windup -= dt;
						if (enemy.windup <= 0.0F) {
							enemy.windup = 0.0F;
							enemy.charge = type.charge_time;
							const glm::vec2 aim = target - position;
							const float aimLength = glm::length(aim);
							enemy.charge_dir = aimLength > 0.0001F ? aim / aimLength : glm::vec2{0.0F};
							SetTint(self, kChargeColor);
						}
					} else if (enemy.charge > 0.0F) {
						enemy.charge -= dt;
						if (enemy.charge <= 0.0F) {
							enemy.charge = 0.0F;
							SetTint(self, enemy.color);
						}
					}
					const bool tumbling = enemy.tumble > 0.0F;
					const bool winding = enemy.windup > 0.0F;
					const bool charging = enemy.charge > 0.0F;
					transform.position.y = wave.height * constants::kWaterHeightScale;
					transform.rotation.x = tumbling ? transform.rotation.x + constants::kTumbleSpin * dt : 0.0F;
					const glm::vec2 toTarget = target - position;
					const float distance = glm::length(toTarget);
					glm::vec2 heading = distance > 0.0001F ? toTarget / distance : glm::vec2{0.0F};
					if (charging) {
						heading = enemy.charge_dir;
					}
					if (winding) {
						// A crouch-and-shiver tell.
						transform.position.y -= constants::kWindupCrouch;
						transform.rotation.z = std::sin(enemy.windup * constants::kWindupShiverRate) * constants::kWindupShiver;
					} else {
						transform.rotation.z = 0.0F;
					}

					glm::vec2 push{0.0F};
					bool blocked = false;
					grid.ForEachNear(position, enemy.radius + grid.max_radius, [&](const CrowdEntry& other) {
						const glm::vec2 apart = position - other.position;
						const float gap = glm::length(apart);
						const float touching = (enemy.radius + other.radius) * kSeparationSlack;
						if (other.entity != self.id() && gap > 0.0001F && gap < touching) {
							push += apart * ((touching - gap) / gap);
							// A neighbour in the cone ahead blocks the way: queue behind it instead of pushing in.
							blocked = blocked || (!tumbling && glm::dot(-apart, heading) > kBlockedCone * gap);
						}
					});

					// A charge pushes through the crowd.
					float speed = enemy.speed;
					if (charging) {
						blocked = false;
						speed *= 1.0F + (type.charge_mult - 1.0F) * (enemy.charge / std::max(0.01F, type.charge_time));
					}
					const float step = blocked || tumbling || winding ? 0.0F : speed * dt;
					const glm::vec2 free =
						position + heading * step + enemy.knock * dt + drift + push * constants::kSeparationRelax;
					const float edge = constants::kArenaRadius + kEdgeSlack;
					if (tumbling && glm::length(free) > edge) {
						// Touching the barrier ends the ride at once: the wind-up starts there.
						endRide();
					}
					glm::vec2 settled = ClampToRadius(free, edge);
					if (PushOutOfRocks(rocks, settled, enemy.radius) && tumbling) {
						// A rock slam: the harder the crest threw it, the harder it hits.
						const float speed = glm::length(enemy.knock);
						if (speed > constants::kSlamMinSpeed) {
							enemy.hp -= (speed - constants::kSlamSpeedOffset) * constants::kSlamDamage;
							enemy.flash = constants::kSlamFlashTime;
							enemy.slammed = true;
							SetTint(it.entity(i), kFlashColor);
						}
						enemy.knock *= constants::kSlamBounce;
					}
					transform.position.x = settled.x;
					transform.position.z = settled.y;
					if (distance > 0.0001F && !tumbling) {
						// Sideways walkers face 90° off the direction of travel, the rest face it.
						const float facing = std::atan2(heading.x, heading.y);
						transform.rotation.y = enemy.walks_sideways ? facing + std::numbers::pi_v<float> / 2.0F : facing;
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
			const auto& types = world.get<EnemyRegistry>().types;
			float damage = 0.0F;
			grid.ForEachNear(position, grid.max_radius + constants::kPlayerContactRadius, [&](const CrowdEntry& e) {
				const Enemy& enemy = world.entity(e.entity).get<Enemy>();
				if (enemy.tumble <= 0.0F && glm::length(e.position - position) < e.radius + constants::kPlayerContactRadius) {
					const EnemyType& type = types[static_cast<size_t>(enemy.type)];
					damage += enemy.contact_dps * (enemy.charge > 0.0F ? type.charge_dps_mult : 1.0F);
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
			auto& run = world.get_mut<RunState>();
			++run.kills;
			run.rock_kills += enemy.slammed ? 1 : 0;
			PlaySfx(world, enemy.slammed ? Sfx::RockCrush : Sfx::EnemyDefeat);
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

glm::vec2 EdgeLanding(const flecs::world& world, const glm::vec2 from, const glm::vec2 target) {
	auto& rng = world.get_mut<ecs::RngState>();
	const glm::vec2 toTarget = target - from;
	const float distance = glm::length(toTarget);
	const glm::vec2 heading = distance > 0.0001F ? toTarget / distance : glm::vec2{0.0F, 1.0F};
	// Where the line first crosses the arena circle: solve |from + t * heading| = radius for the smaller t.
	const float b = glm::dot(from, heading);
	const float discriminant = b * b - (glm::dot(from, from) - constants::kArenaRadius * constants::kArenaRadius);
	const float t = discriminant >= 0.0F ? std::max(0.0F, -b - std::sqrt(discriminant)) : 0.0F;
	const glm::vec2 crossing = from + heading * t;
	const float reach =
		std::min(rng.NextFloatRange(constants::kLandingMin, constants::kLandingMax), std::max(0.0F, distance - t));
	const glm::vec2 jitter{
		rng.NextFloatRange(-constants::kThrowJitter, constants::kThrowJitter),
		rng.NextFloatRange(-constants::kThrowJitter, constants::kThrowJitter)
	};
	return ClampToRadius(crossing + heading * reach + jitter, constants::kArenaRadius - kLandingMargin);
}

glm::vec2 BodyLanding(const flecs::world& world, const glm::vec2 from, const glm::vec2 target) {
	auto& rng = world.get_mut<ecs::RngState>();
	const glm::vec2 toTarget = target - from;
	const float distance = glm::length(toTarget);
	const glm::vec2 heading = distance > 0.0001F ? toTarget / distance : glm::vec2{0.0F, 1.0F};
	const float reach = rng.NextFloatRange(constants::kThrowMinDistance, constants::kThrowMaxDistance);
	const glm::vec2 jitter{
		rng.NextFloatRange(-constants::kThrowJitter, constants::kThrowJitter),
		rng.NextFloatRange(-constants::kThrowJitter, constants::kThrowJitter)
	};
	return ClampToRadius(from + heading * reach + jitter, constants::kArenaRadius - kLandingMargin);
}

void ThrowEnemy(
	const flecs::world& world, const flecs::entity parent, const int typeIndex, const glm::vec2 from, const glm::vec2 landing
) {
	const float flightTime = world.get_mut<ecs::RngState>().NextFloatRange(constants::kThrowMinTime, constants::kThrowMaxTime);
	std::ignore = ThrowEnemyAt(world, parent, typeIndex, from, landing, flightTime);
}

flecs::entity ThrowEnemyAt(
	const flecs::world& world, const flecs::entity parent, const int typeIndex, const glm::vec2 from, const glm::vec2 landing,
	const float flightTime
) {
	const EnemyType& type = world.get<EnemyRegistry>().types.at(static_cast<size_t>(typeIndex));

	// Launch speed for a flight time T from height y0 under gravity G (the prototype's formula).
	const float y0 = constants::kThrowHeight;
	const glm::vec2 ground = (landing - from) / flightTime;
	const float vy = (0.5F * constants::kGravity * flightTime * flightTime - y0) / flightTime;

	const float hpScale = CurrentEnemyHpScale(world);
	const flecs::entity enemy =
		world.entity()
			.child_of(parent)
			// First, so the world transform the engine seeds when it is added starts at the throw point, not the origin.
			.set<spatial::Transform>({.position = {from.x, y0, from.y}})
			.set<Enemy>(
				{.hp = type.hp * hpScale,
				 .speed = type.speed,
				 .radius = type.radius,
				 .contact_dps = type.contact_dps,
				 .xp = type.xp,
				 .type = typeIndex,
				 .tumble_min = type.tumble_min,
				 .walks_sideways = type.walks_sideways,
				 .color = type.color}
			)
			.set<Airborne>({.velocity = {ground.x, vy, ground.y}, .landing = landing});
	// The same body and claws for every type, scaled by the type.
	const float scale = type.scale;
	AddPart(world, enemy, kBodyOffset * scale, kBodyScale * scale, scale, type.color);
	AddPart(world, enemy, kClawOffset * glm::vec3{-1.0F, 1.0F, 1.0F} * scale, glm::vec3{1.0F}, kClawRadius * scale, type.color);
	AddPart(world, enemy, kClawOffset * scale, glm::vec3{1.0F}, kClawRadius * scale, type.color);

	// The landing marker, under the horde so a cleared horde takes it too.
	std::ignore = world.entity()
					  .child_of(parent)
					  .set<spatial::Transform>(
						  {.position = {landing.x, kShadowY, landing.y},
						   .scale = {kShadowStartScale, kShadowFlatness, kShadowStartScale}}
					  )
					  .set<LandingShadow>({.enemy = enemy.id(), .duration = flightTime})
					  .set<render::SpherePrimitive>({.radius = scale * kShadowRadiusK})
					  .set<render::Material>({.color = kShadowColor, .cast_shadow = false, .unlit = true})
					  .add<render::Translucent>();
	return enemy;
}

void DamageEnemy(const flecs::entity enemy, const float amount) {
	auto& state = enemy.get_mut<Enemy>();
	state.hp -= amount;
	state.flash = constants::kHitFlashTime;
	SetTint(enemy, kFlashColor);
}

} // namespace crabnado
