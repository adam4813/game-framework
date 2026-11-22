#include "captain.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

#include "arena.hpp"
#include "boss.hpp"
#include "boss_registry.hpp"
#include "campaign.hpp"
#include "campaign_script.hpp"
#include "crabnado.hpp"
#include "crabnado_constants.hpp"
#include "enemy_types.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"
#include "infested.hpp"
#include "player.hpp"
#include "run.hpp"
#include "sfx.hpp"
#include "waves.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

namespace {

// The seahorses his crest brings in with him.
constexpr std::string_view kEntryRider = "seahorse";
// His look: a big seahorse with the captain on its back. The seahorse's tint shows his state.
constexpr core::Rgba kMountColor{.r = 40, .g = 170, .b = 160, .a = 255};
constexpr core::Rgba kSurfColor{.r = 90, .g = 180, .b = 255, .a = 255};
constexpr core::Rgba kBeachedColor{.r = 255, .g = 240, .b = 150, .a = 255};
constexpr core::Rgba kWindupColor{.r = 255, .g = 200, .b = 40, .a = 255};
constexpr core::Rgba kDashColor{.r = 255, .g = 40, .b = 90, .a = 255};
constexpr core::Rgba kCrabColor{.r = 200, .g = 40, .b = 30, .a = 255};
constexpr core::Rgba kHatColor{.r = 25, .g = 30, .b = 60, .a = 255};
constexpr core::Rgba kLaneColor{.r = 255, .g = 214, .b = 40, .a = 120};
constexpr float kLaneHeight = 0.15F;
constexpr float kLaneThickness = 0.05F;
// His model is kCaptainScale times his radius.
constexpr float kCaptainScale = 0.9F;
// Beached, he lies over this far (radians).
constexpr float kBeachedTilt = 0.6F;
// Escaping, he swims out at kEscapeSpeed units per second and sinks kEscapeSink units per second.
constexpr float kEscapeSpeed = 16.0F;
constexpr float kEscapeSink = 1.5F;
// Surfing, he gains kPeakClimb units per second per unit of height the water is higher kPeakProbe units ahead than behind.
constexpr float kPeakClimb = 8.0F;
constexpr float kPeakProbe = 1.0F;
// The second of his phases: the split.
constexpr int kSplitPhase = 2;

glm::vec2 Normalized(const glm::vec2 v, const glm::vec2 fallback) {
	const float length = glm::length(v);
	return length > 0.0001F ? v / length : fallback;
}

// The compass bearing (as Rogue::direction) of a ground direction: BearingDirection's inverse.
float CompassOf(const glm::vec2 direction) { return std::atan2(direction.x, -direction.y); }

const BossDef* CaptainBoss(const flecs::world& world) {
	const BossDef* boss = CurrentBoss(world);
	return boss != nullptr && boss->behavior == kCaptainBehavior ? boss : nullptr;
}

void TintMount(const flecs::entity captain, const core::Rgba color) {
	captain.children([color](const flecs::entity part) {
		if (part.has<CaptainMount>()) {
			part.get_mut<render::Material>().color = color;
		}
	});
}

void ClearLane(const flecs::world& world, CaptainState& cap) {
	if (cap.lane != 0 && world.is_alive(cap.lane)) {
		world.entity(cap.lane).destruct();
	}
	cap.lane = 0;
}

void PlaceLane(
	const flecs::world& world,
	const CaptainState& cap,
	const glm::vec2 from,
	const float length,
	const float width
) {
	if (cap.lane == 0 || !world.is_alive(cap.lane)) {
		return;
	}
	const glm::vec2 centre = from + cap.aim * (length / 2.0F);
	world.entity(cap.lane).set<spatial::Transform>(
		{.position = {centre.x, kLaneHeight, centre.y}, .rotation = {0.0F, std::atan2(cap.aim.x, cap.aim.y), 0.0F}}
	);
	world.entity(cap.lane).set<render::CubePrimitive>({.size = {width, kLaneThickness, length}});
}

// A rogue crest along `direction` whose front reaches `point` `lead` seconds from now, at `amp`. False with every slot taken.
bool AddRogueThrough(
	WaveField& field,
	const glm::vec2 point,
	const glm::vec2 direction,
	const float lead,
	const float amp
) {
	if (field.rogue_count >= kMaxRogues) {
		return false;
	}
	field.rogues[static_cast<size_t>(field.rogue_count++)] = {
		.direction = CompassOf(direction),
		.t0 = field.time + lead - glm::dot(point, direction) / constants::kWaveSpeed,
		.amp = amp
	};
	return true;
}

// Onto a crest: surfing along `direction`. For `immune` seconds a rock he is pushed against isn't a slam, so a crest
// rising under him beside a rock can't slam him without the player's doing.
void Surf(
	const flecs::entity captain,
	const CaptainTunables& tune,
	BossState& state,
	CaptainState& cap,
	const glm::vec2 direction
) {
	state.action = BossAction::Chase;
	cap.surf_dir = direction;
	cap.lifting = false;
	cap.immune = tune.immune;
	TintMount(captain, kSurfColor);
}

// Off the crest, onto the sand: the weapon window opens for `beach_max` seconds.
void Beach(
	const flecs::world& world,
	const flecs::entity captain,
	const CaptainTunables& tune,
	BossState& state,
	CaptainState& cap
) {
	ClearLane(world, cap);
	state.action = BossAction::Dazed;
	state.timer = tune.beach_max;
	cap.immune = tune.immune;
	cap.lifting = false;
	TintMount(captain, kBeachedColor);
}

// What the fight reads about the player this frame.
struct Target {
	flecs::entity player;
	glm::vec2 position{0.0F};
	glm::vec2 heading{0.0F, 1.0F}; // unit vector from him to the player
	float distance{0.0F};
};

// The crabnado splits: the original slides a quarter turn one way and the twin the other, both holding their throws.
void Split(const flecs::world& world, const BossDef& boss, CaptainState& cap) {
	const CaptainTunables& tune = boss.captain;
	cap.split = true;
	cap.twin_timer = tune.split_pause + tune.twin_gap;
	world.query_builder<Crabnado, FunnelStorm>().without<TwinCrabnado>().build().each(
		[&](const flecs::entity crabnado, Crabnado& orbit, FunnelStorm& storm) {
			const float half = std::numbers::pi_v<float> / 2.0F;
			std::ignore = SpawnTwin(world, crabnado, orbit.bearing + half, tune.split_pause).add<TwinCrabnado>();
			orbit.share = 0.5F;
			orbit.hold = tune.split_pause;
			orbit.slide_to = orbit.bearing - half;
			orbit.slide_left = tune.split_pause;
			storm.flash = 1.0F;
		}
	);
	ecs::Log(world).info("[Run] The crabnado splits at {:.0f} HP", world.get<BossState>().hp);
	FireCampaignEvent(world, "OnBossPhase", {boss.id, kSplitPhase});
}

// Beaten: the field is swept, the twins hold their throws for good, and he starts his escape.
void Defeat(const flecs::world& world, const BossDef& boss, CaptainState& cap) {
	ClearLane(world, cap);
	cap.defeated = true;
	cap.escape = 0.0F;
	SweepBossField(world);
	world.query_builder<Crabnado>().build().each([](Crabnado& crabnado) { crabnado.hold = 1.0e9F; });
	PlaySfx(world, Sfx::BossSlam);
	ecs::Log(world).info("[Run] Captain Crab is beaten at {:.1f}s", world.get<RunState>().clock);
	FireCampaignEvent(world, "OnBossDefeated", {boss.id});
}

// He swims out to sea and sinks; once gone the Act clears.
void StepEscape(
	const flecs::world& world,
	const flecs::entity captain,
	const CaptainTunables& tune,
	CaptainState& cap,
	spatial::Transform& transform,
	const float dt
) {
	cap.escape += dt;
	const glm::vec2 out = Normalized({transform.position.x, transform.position.z}, {1.0F, 0.0F});
	transform.position.x += out.x * kEscapeSpeed * dt;
	transform.position.z += out.y * kEscapeSpeed * dt;
	transform.position.y -= kEscapeSink * dt;
	if (cap.escape >= tune.escape_time) {
		captain.destruct();
		ClearAct(world);
	}
}

void StepCaptain(
	const flecs::world& world,
	const flecs::entity captain,
	spatial::Transform& transform,
	const float dt
) {
	const BossDef* boss = CaptainBoss(world);
	auto& run = world.get_mut<RunState>();
	if (boss == nullptr || !IsRunning(run.phase)) {
		return;
	}
	const CaptainTunables& tune = boss->captain;
	const SlamTunables& body = boss->slam;
	auto& state = world.get_mut<BossState>();
	auto& cap = world.get_mut<CaptainState>();
	if (cap.defeated) {
		StepEscape(world, captain, tune, cap, transform, dt);
		return;
	}

	glm::vec2 position{transform.position.x, transform.position.z};
	Target target{.player = FindPlayer(world), .position = position};
	if (target.player) {
		const glm::vec3 p = target.player.get<spatial::Transform>().position;
		target.position = {p.x, p.z};
		target.distance = glm::length(target.position - position);
		target.heading = Normalized(target.position - position, target.heading);
	}
	const auto& field = world.get<WaveField>();
	const auto& pools = world.get<TidePools>().pools;
	// Pools calm the crests that could grab him while beached; his own crest (and the lift) carries him over them.
	const WaveSample calmed = SampleWaves(field, pools, position);
	const WaveSample open = SampleWaves(field, std::span<const TidePool>{}, position);
	cap.immune = std::max(0.0F, cap.immune - dt);
	cap.lunge_cooldown = std::max(0.0F, cap.lunge_cooldown - dt);
	cap.slam_cooldown = std::max(0.0F, cap.slam_cooldown - dt);

	glm::vec2 velocity{0.0F};
	float contact = body.contact_dps;
	bool slammable = false;
	switch (state.action) {
	case BossAction::Chase:
	{
		// Seahorse rules: off once the crest has passed the player or fallen away.
		const bool passed = target.player && glm::dot(target.position - position, cap.surf_dir) < 0.0F;
		if (open.height < tune.surf_min_height || passed) {
			Beach(world, captain, tune, state, cap);
			break;
		}
		const glm::vec2 side{-cap.surf_dir.y, cap.surf_dir.x};
		const float across = glm::dot(target.position - position, side);
		const float steer = dt > 0.0001F ? std::clamp(across / dt, -tune.surf_steer, tune.surf_steer) : 0.0F;
		// He works toward the crest's peak: picked up on its leading edge, riding at wave speed alone would keep him there.
		const auto heightAt = [&](const float along) {
			return SampleWaves(field, std::span<const TidePool>{}, position + cap.surf_dir * along).height;
		};
		const float climb = kPeakClimb * (heightAt(kPeakProbe) - heightAt(-kPeakProbe));
		velocity = cap.surf_dir * (constants::kWaveSpeed + climb) + side * steer;
		contact *= tune.surf_dps_mult;
		slammable = true;
		if (target.player && !cap.outside && target.distance < tune.lunge_range && cap.lunge_cooldown <= 0.0F) {
			state.action = BossAction::Windup;
			state.timer = tune.lunge_windup;
			cap.aim = target.heading;
			cap.lane = world.entity()
						   .child_of(captain.parent())
						   .set<spatial::Transform>({.position = {position.x, kLaneHeight, position.y}})
						   .set<render::CubePrimitive>({.size = {2.0F * body.radius, kLaneThickness, tune.lunge_dist}})
						   .set<render::Material>({.color = kLaneColor, .cast_shadow = false, .unlit = true})
						   .add<render::Translucent>()
						   .id();
			TintMount(captain, kWindupColor);
		}
		break;
	}
	case BossAction::Windup:
		// The lane tracks the player until the dash locks it.
		state.timer -= dt;
		cap.aim = target.heading;
		PlaceLane(world, cap, position, tune.lunge_dist, 2.0F * body.radius);
		if (state.timer <= 0.0F) {
			ClearLane(world, cap);
			state.action = BossAction::Active;
			cap.dash_left = tune.lunge_dist;
			TintMount(captain, kDashColor);
		}
		break;
	case BossAction::Active:
		velocity = cap.aim * tune.lunge_speed;
		contact *= tune.lunge_dps_mult;
		slammable = true;
		cap.dash_left -= tune.lunge_speed * dt;
		if (cap.dash_left <= 0.0F) {
			state.action = BossAction::Recover;
			state.timer = tune.recover;
			cap.lunge_cooldown = tune.lunge_cooldown;
			TintMount(captain, kMountColor);
		}
		break;
	case BossAction::Recover:
	case BossAction::Dazed:
		state.timer -= dt;
		if (state.action == BossAction::Dazed) {
			contact = 0.0F;
		}
		// The window's cap, or a missed lunge's end: a lift crest rises under him and rolls at the player.
		if (state.timer <= 0.0F && !cap.lifting) {
			cap.lifting =
				AddRogueThrough(world.get_mut<WaveField>(), position, target.heading, tune.lift_lead, tune.lift_amp);
			cap.surf_dir = target.heading;
		}
		if (cap.lifting && open.height > tune.surf_min_height) {
			Surf(captain, tune, state, cap, cap.surf_dir);
		}
		else if (cap.immune <= 0.0F && calmed.height > tune.grab_height) {
			// A big crest (a twin's rogue) picks him up if it is rolling toward the player.
			const glm::vec2 direction = Normalized(calmed.push, target.heading);
			if (!target.player || glm::dot(target.position - position, direction) > 0.0F) {
				Surf(captain, tune, state, cap, direction);
			}
		}
		break;
	}

	position += velocity * dt;
	const float reach = constants::kArenaRadius - body.edge_margin;
	if (cap.outside && glm::length(position) <= reach) {
		cap.outside = false;
	}
	if (!cap.outside && glm::length(position) > reach) {
		position *= reach / glm::length(position);
	}
	if (PushOutOfRocks(world.get<Rocks>().circles, position, body.radius)
		&& slammable
		&& cap.immune <= 0.0F
		&& glm::length(velocity) > body.slam_min_speed
		&& cap.slam_cooldown <= 0.0F) {
		state.hp -= tune.slam_damage;
		cap.slam_cooldown = body.slam_cooldown;
		++run.slams;
		run.banner_timer = constants::kBossBannerTime;
		PlaySfx(world, Sfx::BossSlam);
		ecs::Log(world).info("[Run] Captain Crab slammed: HP {:.0f}/{:.0f}", state.hp, state.max_hp);
		Beach(world, captain, tune, state, cap);
	}
	if (target.player && target.distance < body.contact_range && contact > 0.0F) {
		float& hp = target.player.get_mut<PlayerStats>().hp;
		hp = std::max(0.0F, hp - contact * dt);
	}

	transform.position = {position.x, open.height * constants::kWaterHeightScale, position.y};
	const glm::vec2 facing = state.action == BossAction::Chase ? cap.surf_dir : cap.aim;
	if (glm::length(facing) > 0.0001F) {
		transform.rotation.y = std::atan2(facing.x, facing.y);
	}
	transform.rotation.z = state.action == BossAction::Dazed ? kBeachedTilt : 0.0F;
	state.position = position;
	state.radius = body.radius;

	// Weapons take HP in their own systems, so both thresholds are checked here, every frame.
	if (!cap.split && state.hp <= state.max_hp * tune.split_at) {
		Split(world, *boss, cap);
	}
	if (state.hp <= 0.0F) {
		Defeat(world, *boss, cap);
	}
}

// Brings him in: the next crest is infested to carry him, or, with no swell to ride, he lands at the upwind barrier.
void CallEntry(const flecs::world& world, const BossDef& boss) {
	const auto& types = world.get<EnemyRegistry>().types;
	const auto rider = std::ranges::find(types, kEntryRider, &EnemyType::id);
	const int riderType = rider == types.end() ? 0 : static_cast<int>(rider - types.begin());
	const int riders = rider == types.end() ? 0 : boss.captain.entry_riders;
	if (InfestForCaptain(world, riderType, riders)) {
		return;
	}
	ecs::Log(world).warn("[Run] No swell to carry Captain Crab: he lands at the barrier");
	const glm::vec2 downwind = WindDirection(world.get<Wind>());
	if (const flecs::entity horde = FindHorde(world)) {
		SpawnCaptain(world, horde.parent(), -downwind * (constants::kArenaRadius - boss.slam.edge_margin), downwind);
	}
}

// The twins' turn: one strobes through its wind-up, then throws a volley or summons a rogue at the player. The attack
// alternates on top of the twin, so each twin does both.
void StepTwins(const flecs::world& world, const CaptainTunables& tune, CaptainState& cap, const float dt) {
	std::vector<flecs::entity> twins;
	world.query_builder<const Crabnado>().build().each([&twins](const flecs::entity crabnado, const Crabnado&) {
		twins.push_back(crabnado);
	});
	if (twins.size() < 2) {
		return;
	}
	std::ranges::sort(twins, [](const flecs::entity a, const flecs::entity b) { return a.id() < b.id(); });
	cap.twin_timer -= dt;
	const flecs::entity attacker = twins[static_cast<size_t>(cap.twin_turn % 2)];
	if (cap.twin_timer <= tune.twin_windup) {
		attacker.get_mut<FunnelStorm>().flash =
			std::fmod(cap.twin_timer * constants::kWindupStrobeHz, 1.0F) < 0.5F ? 0.0F : 1.0F;
	}
	if (cap.twin_timer > 0.0F) {
		return;
	}
	const glm::vec3 at = attacker.get<spatial::Transform>().position;
	const glm::vec2 from{at.x, at.z};
	glm::vec2 player{0.0F};
	if (const flecs::entity p = FindPlayer(world)) {
		player = {p.get<spatial::Transform>().position.x, p.get<spatial::Transform>().position.z};
	}
	const bool volley = (cap.twin_turn + cap.twin_turn / 2) % 2 == 0;
	if (volley) {
		ThrowVolley(
			world,
			from,
			player,
			{.ring = tune.volley_ring,
			 .radius = tune.volley_radius,
			 .flight = tune.volley_flight,
			 .impact = tune.volley_impact}
		);
	}
	else if (
		AddRogueThrough(world.get_mut<WaveField>(), from, Normalized(player - from, {1.0F, 0.0F}), 0.0F, tune.rogue_amp)
	) {
		// It rises at the barrier below the twin and rolls at the player.
		PlaySfx(world, Sfx::RogueWarning);
	}
	++cap.twin_turn;
	cap.twin_timer = tune.twin_gap;
}

// The twins collapse together as he escapes.
void StepCollapse(const flecs::world& world, const CaptainTunables& tune, const CaptainState& cap) {
	const float scale = std::max(0.0F, 1.0F - cap.escape / std::max(0.01F, tune.escape_time));
	world.query_builder<const Crabnado, spatial::Transform>().build().each(
		[scale](const flecs::entity crabnado, const Crabnado&, spatial::Transform& transform) {
			transform.scale = glm::vec3{scale};
			if (scale <= 0.0F) {
				crabnado.disable();
			}
		}
	);
}

// Leaving the Boss Stage (a Remote API jump) takes him and the twin away and puts the crabnado back to one.
void EndFight(const flecs::world& world, CaptainState& cap) {
	if (!cap.entry_called && !cap.arrived) {
		return;
	}
	ClearLane(world, cap);
	world.delete_with<CaptainCrab>();
	world.delete_with<TwinCrabnado>();
	world.query_builder<Crabnado, spatial::Transform>().build().each([](Crabnado& crabnado,
																		spatial::Transform& transform) {
		crabnado.share = 1.0F;
		crabnado.hold = 0.0F;
		crabnado.slide_left = 0.0F;
		transform.scale = glm::vec3{1.0F};
	});
	world.get_mut<BossState>() = {};
	cap = {};
}

void StepCaptainStage(const flecs::world& world, const float dt) {
	auto& cap = world.get_mut<CaptainState>();
	const BossDef* boss = CaptainBoss(world);
	if (boss == nullptr || !InBossStage(world)) {
		EndFight(world, cap);
		return;
	}
	const auto& run = world.get<RunState>();
	if (!IsRunning(run.phase)) {
		return;
	}
	// He is called as the storm rises, or at once if a script brought the boss early.
	if (!cap.entry_called && (run.storm_up > 0.0F || run.phase == RunPhase::Boss)) {
		cap.entry_called = true;
		CallEntry(world, *boss);
	}
	if (cap.defeated) {
		StepCollapse(world, boss->captain, cap);
		return;
	}
	if (cap.split && run.phase == RunPhase::Boss) {
		StepTwins(world, boss->captain, cap, dt);
	}
}

} // namespace

void SpawnCaptain(
	const flecs::world& world,
	const flecs::entity parent,
	const glm::vec2 position,
	const glm::vec2 direction
) {
	const BossDef* boss = CaptainBoss(world);
	if (boss == nullptr) {
		return;
	}
	const CaptainTunables& tune = boss->captain;
	world.get_mut<BossState>() = {
		.action = BossAction::Chase,
		.hp = tune.hp,
		.max_hp = tune.hp,
		.position = position,
		.radius = boss->slam.radius
	};
	auto& cap = world.get_mut<CaptainState>();
	cap.arrived = true;
	cap.outside = true;
	cap.surf_dir = direction;
	// About twice a seahorse: he is a big mount with the captain riding high.
	const float scale = boss->slam.radius * kCaptainScale;
	const flecs::entity captain = world.entity("CaptainCrab")
									  .child_of(parent)
									  .set<spatial::Transform>({.position = {position.x, 0.0F, position.y}})
									  .add<CaptainCrab>();
	const auto part = [&](const glm::vec3 at, const glm::vec3 size, const core::Rgba color) {
		return world.entity()
			.child_of(captain)
			.set<spatial::Transform>({.position = at * scale, .scale = size * scale})
			.set<render::SpherePrimitive>({.radius = 1.0F})
			.set<render::Material>({.color = color});
	};
	// The seahorse: a tall body, a head leaning forward (+z) and a curled tail behind.
	std::ignore = part({0.0F, 1.6F, 0.0F}, {0.9F, 1.8F, 0.9F}, kSurfColor).add<CaptainMount>();
	std::ignore = part({0.0F, 3.4F, 0.6F}, {0.6F, 0.6F, 1.0F}, kSurfColor).add<CaptainMount>();
	std::ignore = part({0.0F, 0.4F, -0.9F}, {0.4F, 0.4F, 0.6F}, kSurfColor).add<CaptainMount>();
	// The captain on its back: a crab body, two claws (one raised) and a dark hat.
	std::ignore = part({0.0F, 3.0F, -0.4F}, {1.1F, 0.55F, 0.8F}, kCrabColor);
	std::ignore = part({-1.0F, 3.3F, 0.2F}, {0.35F, 0.35F, 0.35F}, kCrabColor);
	std::ignore = part({1.0F, 4.0F, 0.2F}, {0.35F, 0.35F, 0.35F}, kCrabColor);
	std::ignore = part({0.0F, 3.65F, -0.4F}, {0.6F, 0.25F, 0.6F}, kHatColor);
	ecs::Log(world).info("[Run] Captain Crab rides in at {:.1f}s", world.get<RunState>().clock);
}

void ResetCaptain(const flecs::world& world) { world.set<CaptainState>({}); }

void RegisterCaptain(const flecs::world& world) {
	world.component<CaptainCrab>();
	world.component<CaptainMount>();
	world.component<TwinCrabnado>();
	world.component<CaptainState>()
		.member<bool>("entry_called")
		.member<bool>("arrived")
		.member<bool>("outside")
		.member<bool>("lifting")
		.member<glm::vec2>("surf_dir")
		.member<glm::vec2>("aim")
		.member<float>("dash_left")
		.member<uint64_t>("lane")
		.member<float>("immune")
		.member<float>("lunge_cooldown")
		.member<float>("slam_cooldown")
		.member<bool>("split")
		.member<float>("twin_timer")
		.member<int>("twin_turn")
		.member<bool>("defeated")
		.member<float>("escape");
	world.set<CaptainState>({});

	world.system<spatial::Transform>("CaptainFight")
		.with<CaptainCrab>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, spatial::Transform& transform) {
			StepCaptain(it.world(), it.entity(i), transform, it.delta_time());
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	world.system("CaptainTwins")
		.kind(flecs::OnUpdate)
		.run([](flecs::iter& it) { StepCaptainStage(it.world(), it.delta_time()); })
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

} // namespace crabnado
