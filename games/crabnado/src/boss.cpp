#include "boss.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include <spdlog/spdlog.h>

#include "arena.hpp"
#include "boss_registry.hpp"
#include "campaign_script.hpp"
#include "crabnado.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "enemy_types.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "run.hpp"
#include "sfx.hpp"
#include "shells.hpp"
#include "waves.hpp"

using namespace engine;

namespace crabnado {

namespace {

// The enemy type a volley throws: an ordinary horde crab.
constexpr std::string_view kVolleyEnemy = "crab";
constexpr core::Rgba kDiscColor{.r = 255, .g = 214, .b = 40, .a = 130};
constexpr core::Rgba kRingColor{.r = 235, .g = 50, .b = 40, .a = 255};
constexpr float kDiscHeight = 0.15F;
constexpr float kDiscThickness = 0.05F;
// How far inside the arena edge a volley landing is held, so no crab lands on the barrier.
constexpr float kVolleyEdgeMargin = 2.0F;
// The suction ring is beads laid along a circle at the pull radius, each a thin sliver along the tangent.
constexpr int kRingBeads = 72;
constexpr float kRingBeadThickness = 0.25F;
constexpr float kRingBeadHeight = 0.12F;
constexpr float kRingBeadOverlap = 1.1F;

// The last slam: the horde is swept away (no shells, no kills, no XP), every shell on the water is vacuumed to the
// player, and the crabnado starts its retreat back over the barrier from where it stands.
void Defeat(const flecs::world& world, Crabnado& crabnado, const spatial::Transform& transform) {
	// Collected first: destroying while a query iterates would change the tables under it.
	std::vector<flecs::entity> horde;
	world.query_builder<const Enemy>().build().each([&horde](const flecs::entity enemy, const Enemy&) { horde.push_back(enemy); });
	for (const flecs::entity enemy : horde) {
		enemy.destruct();
	}
	world.query_builder<Shell>().build().each([](Shell& shell) { shell.flying = true; });
	world.delete_with<BossRing>();

	const glm::vec2 position{transform.position.x, transform.position.z};
	crabnado.mode = CrabnadoMode::Retreat;
	crabnado.mode_time = 0.0F;
	if (const BossDef* boss = CurrentBoss(world)) {
		FireCampaignEvent(world, "OnBossDefeated", {boss->id});
	}
	crabnado.bearing = BearingOf(position);
	crabnado.radius = glm::length(position);
	crabnado.leg_from = crabnado.radius;
	crabnado.knock = glm::vec2{0.0F};
}

// What the enraged fight reads about the crabnado and the player this frame.
struct Foes {
	flecs::entity crabnado;
	glm::vec2 position{0.0F};
	flecs::entity player;
	glm::vec2 playerPosition{0.0F};
	glm::vec2 heading{0.0F}; // unit vector from the crabnado to the player
	float distance{0.0F};
};

float NextChaseGap(const flecs::world& world, const EnragedTunables& tune) {
	return world.get_mut<ecs::RngState>().NextFloatRange(tune.chase_min, tune.chase_max);
}

// Never the same attack twice running: draws from the others.
BossAttack PickAttack(const flecs::world& world, const BossAttack last) {
	constexpr std::array attacks{BossAttack::Volley, BossAttack::Suction};
	std::array<BossAttack, attacks.size()> options{};
	size_t count = 0;
	for (const BossAttack attack : attacks) {
		if (attack != last) {
			options[count++] = attack;
		}
	}
	const float pick = world.get_mut<ecs::RngState>().NextFloat() * static_cast<float>(count);
	return options[std::min(static_cast<size_t>(pick), count - 1)];
}

void AddSuctionRing(const flecs::world& world, const flecs::entity crabnado, const float radius) {
	constexpr float tau = 2.0F * std::numbers::pi_v<float>;
	const flecs::entity ring = world.entity().child_of(crabnado).add<BossRing>().set<spatial::Transform>(
		{.position = {0.0F, kDiscHeight, 0.0F}}
	);
	const float half = tau * radius / kRingBeads / 2.0F * kRingBeadOverlap;
	for (int i = 0; i < kRingBeads; ++i) {
		const float angle = tau * static_cast<float>(i) / kRingBeads;
		// A unit sphere stretched along its local x, yawed so x runs along the circle's tangent.
		std::ignore = world.entity()
						  .child_of(ring)
						  .set<spatial::Transform>(
							  {.position = {std::cos(angle) * radius, 0.0F, std::sin(angle) * radius},
							   .rotation = {0.0F, -angle - std::numbers::pi_v<float> / 2.0F, 0.0F},
							   .scale = {half, kRingBeadHeight, kRingBeadThickness}}
						  )
						  .set<render::SpherePrimitive>({.radius = 1.0F})
						  .set<render::Material>({.color = kRingColor, .cast_shadow = false, .unlit = true});
	}
}

// Throws the volley: a ring of crabs round the player plus one on the player, each with a yellow disc where it lands.
void ThrowVolley(const flecs::world& world, const EnragedTunables& tune, const Foes& foes) {
	const flecs::entity horde = foes.crabnado.target<ThrowsInto>();
	const int typeIndex = static_cast<int>(
		std::ranges::find(world.get<EnemyRegistry>().types, kVolleyEnemy, &EnemyType::id) - world.get<EnemyRegistry>().types.begin()
	);
	if (!horde || typeIndex >= static_cast<int>(world.get<EnemyRegistry>().types.size())) {
		return;
	}
	const float discRadius = world.get<EnemyRegistry>().types[static_cast<size_t>(typeIndex)].radius + constants::kVolleyDiscMargin;
	const float spin = world.get_mut<ecs::RngState>().NextFloatRange(0.0F, 2.0F * std::numbers::pi_v<float>);
	const float reach = constants::kArenaRadius - kVolleyEdgeMargin;
	for (int i = 0; i <= tune.volley_ring; ++i) {
		glm::vec2 at = foes.playerPosition;
		if (i < tune.volley_ring) {
			const float angle = spin + 2.0F * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(tune.volley_ring);
			at += glm::vec2{std::cos(angle), std::sin(angle)} * tune.volley_radius;
		}
		if (glm::length(at) > reach) {
			at *= reach / glm::length(at);
		}
		const flecs::entity crab = ThrowEnemyAt(world, horde, typeIndex, foes.position, at, tune.volley_flight);
		std::ignore = world.entity()
						  .child_of(horde)
						  .set<spatial::Transform>(
							  {.position = {at.x, kDiscHeight, at.y}, .scale = {discRadius, kDiscThickness, discRadius}}
						  )
						  .set<VolleyDisc>({.enemy = crab.id(), .landing = at, .radius = discRadius, .damage = tune.volley_impact})
						  .set<render::SpherePrimitive>({.radius = 1.0F})
						  .set<render::Material>({.color = kDiscColor, .cast_shadow = false, .unlit = true})
						  .add<render::Translucent>();
	}
}

void StartWindup(const flecs::world& world, const EnragedTunables& tune, BossState& state, const Foes& foes) {
	state.attack = PickAttack(world, state.last_attack);
	state.last_attack = state.attack;
	state.action = BossAction::Windup;
	if (state.attack == BossAttack::Volley) {
		// The throws fly during the wind-up: the discs are the telegraph.
		state.timer = tune.volley_flight;
		ThrowVolley(world, tune, foes);
	} else {
		state.timer = tune.suction_warn;
		AddSuctionRing(world, foes.crabnado, tune.suction_radius);
	}
}

void StartRecover(const EnragedTunables& tune, BossState& state) {
	state.action = BossAction::Recover;
	state.attack = BossAttack::None;
	state.timer = tune.recover;
}

// A slam costs HP; it dazes the crabnado unless it is dazed already (the daze isn't extended).
void SlamEnraged(const flecs::world& world, const EnragedTunables& tune, BossState& state) {
	state.hp -= tune.slam_damage;
	if (state.action == BossAction::Dazed) {
		return;
	}
	world.delete_with<BossRing>();
	state.action = BossAction::Dazed;
	state.attack = BossAttack::None;
	state.timer = tune.daze_time;
}

// One tick of the enraged fight. Returns the velocity the crabnado moves itself at (the wave knock is added after);
// `contactDps` is lowered to 0 while it is dazed.
glm::vec2 StepEnraged(
	const flecs::world& world, const EnragedTunables& tune, const SlamTunables& slam, BossState& state, FunnelStorm& storm,
	const Foes& foes, const float clock, const float dt, float& contactDps
) {
	glm::vec2 move{0.0F};
	switch (state.action) {
	case BossAction::Chase:
		move = foes.heading * slam.speed;
		state.gap -= dt;
		if (state.gap <= 0.0F && foes.player) {
			StartWindup(world, tune, state, foes);
		}
		break;
	case BossAction::Windup:
		state.timer -= dt;
		storm.flash = std::fmod(clock * constants::kWindupStrobeHz, 1.0F) < 0.5F ? 0.0F : 1.0F;
		if (state.timer > 0.0F) {
			break;
		}
		if (state.attack == BossAttack::Suction) {
			state.action = BossAction::Active;
			state.timer = tune.suction_time;
		} else {
			StartRecover(tune, state);
		}
		break;
	case BossAction::Active:
		state.timer -= dt;
		// Digging in cancels the pull for the frame; outrunning works since the player is faster than the pull.
		if (foes.player && foes.distance < tune.suction_radius && !foes.player.get<Stamina>().dug_in) {
			const glm::vec2 pull = -foes.heading * std::min(tune.suction_speed * dt, foes.distance);
			glm::vec3& p = foes.player.get_mut<spatial::Transform>().position;
			p.x += pull.x;
			p.z += pull.y;
		}
		if (state.timer <= 0.0F) {
			world.delete_with<BossRing>();
			StartRecover(tune, state);
		}
		break;
	case BossAction::Recover:
		state.timer -= dt;
		if (state.timer <= 0.0F) {
			state.action = BossAction::Chase;
			state.gap = NextChaseGap(world, tune);
		}
		break;
	case BossAction::Dazed:
		state.timer -= dt;
		contactDps = 0.0F;
		storm.flash = constants::kDazedFlash;
		if (state.timer <= 0.0F) {
			state.action = BossAction::Chase;
			state.gap = NextChaseGap(world, tune);
		}
		break;
	}
	return move;
}

void StepBoss(
	const flecs::world& world, const flecs::entity entity, Crabnado& crabnado, spatial::Transform& transform, const float dt
) {
	auto& run = world.get_mut<RunState>();
	const BossDef* boss = CurrentBoss(world);
	if (boss == nullptr) {
		return;
	}
	const SlamTunables& tune = boss->slam;
	const bool enraged = boss->behavior == kEnragedBehavior;
	auto& state = world.get_mut<BossState>();
	// A molt in the middle of the fight pauses it; only a fight that is over or not begun starts the state afresh.
	const bool fighting = run.phase == RunPhase::Boss || (run.phase == RunPhase::Molting && run.resume_phase == RunPhase::Boss);
	if (enraged && !fighting) {
		state = {.gap = boss->enraged.chase_min, .hp = boss->enraged.hp, .max_hp = boss->enraged.hp, .radius = tune.radius};
	}
	if (run.phase != RunPhase::Boss || crabnado.mode != CrabnadoMode::Fight) {
		return;
	}
	run.slam_cooldown = std::max(0.0F, run.slam_cooldown - dt);

	glm::vec2 position{transform.position.x, transform.position.z};
	Foes foes{.crabnado = entity, .position = position, .player = FindPlayer(world)};
	foes.playerPosition = position;
	if (foes.player) {
		const glm::vec3 p = foes.player.get<spatial::Transform>().position;
		foes.playerPosition = {p.x, p.z};
		const glm::vec2 toPlayer = foes.playerPosition - position;
		foes.distance = glm::length(toPlayer);
		if (foes.distance > 0.0001F) {
			foes.heading = toPlayer / foes.distance;
		}
	}

	const WaveSample wave = SampleWaves(world.get<WaveField>(), world.get<TidePools>().pools, position);
	if (wave.height > tune.knock_min_height) {
		crabnado.knock = wave.push * tune.knock_push;
	} else {
		crabnado.knock *= std::exp(-constants::kKnockDecay * dt);
	}

	glm::vec2 move = foes.heading * tune.speed;
	float contactDps = tune.contact_dps;
	if (enraged) {
		move = StepEnraged(world, boss->enraged, tune, state, entity.get_mut<FunnelStorm>(), foes, run.clock, dt, contactDps);
	}
	if (foes.player && foes.distance < tune.contact_range) {
		foes.player.get_mut<PlayerStats>().hp -= contactDps * dt;
	}

	position += (move + crabnado.knock) * dt;
	const float reach = constants::kArenaRadius - tune.edge_margin;
	const float fromCentre = glm::length(position);
	if (fromCentre > reach) {
		position *= reach / fromCentre;
	}
	if (PushOutOfRocks(world.get<Rocks>().circles, position, tune.radius)) {
		if (glm::length(crabnado.knock) > tune.slam_min_speed && run.slam_cooldown <= 0.0F) {
			++run.slams;
			run.slam_cooldown = tune.slam_cooldown;
			run.banner_timer = constants::kBossBannerTime;
			crabnado.knock *= tune.slam_bounce;
			PlaySfx(world, Sfx::BossSlam);
			// Each slam is a new phase of the fight: phase 2 after the first, and so on.
			FireCampaignEvent(world, "OnBossPhase", {boss->id, run.slams + 1});
			if (enraged) {
				SlamEnraged(world, boss->enraged, state);
				spdlog::info("[Run] Slam {}: boss HP {:.0f}/{:.0f}", run.slams, state.hp, state.max_hp);
			} else {
				spdlog::info("[Run] Slam {}/{}", run.slams, tune.slams_to_win);
			}
		}
	}
	transform.position.x = position.x;
	transform.position.z = position.y;
	state.position = position;
	// A dazed crabnado's HP also falls to weapons (DamageBoss), so this is where either kind of death is seen.
	if (enraged ? state.hp <= 0.0F : run.slams >= tune.slams_to_win) {
		Defeat(world, crabnado, transform);
	}
}

} // namespace

const BossState* DazedBoss(const flecs::world& world) {
	const auto& state = world.get<BossState>();
	return state.action == BossAction::Dazed && state.hp > 0.0F ? &state : nullptr;
}

void DamageBoss(const flecs::world& world, const float amount) {
	world.get_mut<BossState>().hp -= amount;
}

void RegisterBoss(const flecs::world& world) {
	world.component<BossAction>()
		.constant("Chase", BossAction::Chase)
		.constant("Windup", BossAction::Windup)
		.constant("Active", BossAction::Active)
		.constant("Recover", BossAction::Recover)
		.constant("Dazed", BossAction::Dazed);
	world.component<BossAttack>()
		.constant("None", BossAttack::None)
		.constant("Volley", BossAttack::Volley)
		.constant("Suction", BossAttack::Suction);
	world.component<BossState>()
		.member<BossAction>("action")
		.member<BossAttack>("attack")
		.member<BossAttack>("last_attack")
		.member<float>("timer")
		.member<float>("gap")
		.member<float>("hp")
		.member<float>("max_hp")
		.member<glm::vec2>("position")
		.member<float>("radius");
	world.component<VolleyDisc>()
		.member<uint64_t>("enemy")
		.member<glm::vec2>("landing")
		.member<float>("radius")
		.member<float>("damage");
	world.component<BossRing>();
	world.set<BossState>({});

	world.system<Crabnado, spatial::Transform>("BossUpdate")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, Crabnado& crabnado, spatial::Transform& transform) {
			StepBoss(it.world(), it.entity(i), crabnado, transform, it.delta_time());
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// A disc goes when its crab lands or is gone (a swept horde, a Retry); a landing on the player hurts once.
	world.system<const VolleyDisc>("VolleyDiscLand")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, const VolleyDisc& disc) {
			const flecs::world world = it.world();
			if (world.is_alive(disc.enemy) && world.entity(disc.enemy).has<Airborne>()) {
				return;
			}
			if (world.is_alive(disc.enemy)) {
				if (const flecs::entity player = FindPlayer(world)) {
					const glm::vec3 p = player.get<spatial::Transform>().position;
					if (glm::length(glm::vec2{p.x, p.z} - disc.landing) < disc.radius) {
						player.get_mut<PlayerStats>().hp -= disc.damage;
					}
				}
			}
			it.entity(i).destruct();
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

} // namespace crabnado
