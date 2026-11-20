#include "crabnado.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "arena.hpp"
#include "boss_registry.hpp"
#include "campaign.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "enemy_types.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "run.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

namespace {

// Look: a wispy funnel widening upward, made of curved tubes of cloud curled around it in rings, each ring
// twisted a little further round than the one below, with gaps between the tubes, and cloud puffs inside. Crab-red
// specks are caught in it. The whole crabnado spins, which swirls the strands and carries the specks round.
constexpr core::Rgba kFunnelColor{.r = 196, .g = 210, .b = 220, .a = 255};
constexpr core::Rgba kWispColor{.r = 245, .g = 249, .b = 252, .a = 255};
constexpr core::Rgba kSpeckColor{.r = 216, .g = 69, .b = 43, .a = 255};
constexpr int kFunnelRings = 18;
constexpr int kWispsPerRing = 7;
constexpr float kFunnelRingHeight = 1.2F;
constexpr float kFunnelBaseRadius = 1.4F;
constexpr float kFunnelFlare = 0.44F;     // extra radius per ring
constexpr float kFunnelTwist = 0.45F;     // radians each ring is turned past the one below
constexpr float kWispArc = 0.6F;          // share of its slice of the ring a strand covers; the rest is gap
// A strand is a curved tube: kWispSegments lit ellipsoids laid along the ring's arc, overlapping by
// kWispOverlap so they join up, thickest in the middle and tapering to kWispTaper of that at the ends.
constexpr int kWispSegments = 5;
constexpr float kWispOverlap = 2.4F;
constexpr float kWispTubeRadius = 0.32F;
constexpr float kWispTaper = 0.45F;
constexpr float kWispRise = 0.6F;         // how far a strand climbs from end to end, so the rings read as a spiral
constexpr float kWispJitter = 0.3F;       // how far a strand strays in or out of its ring, and up or down
constexpr float kWispMinHalfLength = 0.9F; // keeps the narrow rings near the ground from turning into dots
// Cloud puffs fill the funnel so it isn't hollow: lumpy clusters of kPuffBlobs lit spheres scattered through the
// inside, larger higher up.
// They reach out to kPuffReach of the wall's radius, so some bleed through the strands.
constexpr core::Rgba kPuffColor{.r = 246, .g = 249, .b = 252, .a = 255};
constexpr int kPuffs = 26;
constexpr int kPuffBlobs = 3;
constexpr float kPuffReach = 1.1F;
constexpr float kPuffMinRadius = 0.45F;
constexpr float kPuffMaxRadius = 1.3F;
constexpr int kSpecks = 14;
// Each ring sways in an ellipse up to kSwayReach units wide at the top (none at the base) and twists back and forth
// by kSwayTwist radians, at kSwayRate radians per second. The top lags kSwayLag radians behind the base.
constexpr float kSwayRate = 1.4F;
constexpr float kSwayLag = 2.6F;
constexpr float kSwayReach = 2.2F;
constexpr float kSwayTwist = 0.35F;
constexpr float kSpin = 2.0F; // radians per second

// The ring nearest to `y` units up the funnel.
int NearestRing(const float y) {
	return std::clamp(static_cast<int>(y / kFunnelRingHeight), 0, kFunnelRings - 1);
}

// How high ring `ring`'s pivot sits. Read from here, not the pivot's Transform: a scene load defers the set.
float RingHeight(const int ring) {
	return kFunnelRingHeight * (static_cast<float>(ring) + 0.5F);
}

// The spawner id of the crabnado's own throw (a spawn table entry names the spawner that brings it in).
constexpr std::string_view kThrowSource = "crabnado";

// Throws up to `count` crabs from the crabnado toward the player, stopping at the horde cap.
void ThrowAtPlayer(const flecs::world& world, const flecs::entity crabnado, const glm::vec3 position, int count) {
	count = std::min(count, constants::kHordeCap - world.count<Enemy>());
	const flecs::entity horde = crabnado.target<ThrowsInto>();
	const auto plan = CurrentSpawnPlan(world);
	if (count <= 0 || !horde || !plan) {
		return;
	}
	const glm::vec2 from{position.x, position.z};
	glm::vec2 target = from;
	if (const flecs::entity player = FindPlayer(world)) {
		const glm::vec3 p = player.get<spatial::Transform>().position;
		target = {p.x, p.z};
	}
	// From the orbit it lands in from the barrier; ashore, the body throws close by.
	const bool fromBody = crabnado.get<Crabnado>().mode == CrabnadoMode::Fight;
	for (int i = 0; i < count; ++i) {
		const int type = PickEnemyType(world, *plan->spawns, kThrowSource);
		if (type >= 0) {
			ThrowEnemy(world, horde, type, from, fromBody ? BodyLanding(world, from, target) : EdgeLanding(world, from, target));
		}
	}
}

// Eases `current` toward `target` along the short way round the circle, by `fraction` of the gap.
float EaseBearing(const float current, const float target, const float fraction) {
	constexpr float tau = 2.0F * std::numbers::pi_v<float>;
	const float gap = std::remainder(target - current, tau);
	return current + gap * std::clamp(fraction, 0.0F, 1.0F);
}

// One tick of the orbit mode: circle at the tunable speed (faster in gusts), or in the storm-up's slide stretch glide
// to the bearing of the player's position so the crabnado arrives with kStormHoldTime to spare, then hold.
void StepOrbit(const flecs::world& world, Crabnado& crabnado, const SlamTunables& tune, const float dt) {
	const auto& run = world.get<RunState>();
	crabnado.radius = tune.orbit_radius;
	if (InBossStage(world) && run.storm_up > 0.0F) {
		const float slideLeft = run.storm_up - constants::kStormHoldTime;
		const flecs::entity player = FindPlayer(world);
		if (slideLeft > 0.0F && player) {
			const glm::vec3 p = player.get<spatial::Transform>().position;
			if (glm::length(glm::vec2{p.x, p.z}) > 1.0F) {
				crabnado.bearing = EaseBearing(crabnado.bearing, BearingOf({p.x, p.z}), dt / slideLeft);
			}
		}
		return;
	}
	const float speed = tune.orbit_speed * (1.0F + tune.gust_speed_mult * world.get<Wind>().gust);
	// Counterclockwise on the north-up map is the bearing shrinking.
	crabnado.bearing -= static_cast<float>(crabnado.direction) * speed / std::max(1.0F, crabnado.radius) * dt;
}

// One tick of a Breach or Retreat leg: slide radially from `leg_from` to `to` over `duration`; true once it has arrived.
bool StepLeg(Crabnado& crabnado, const float to, const float duration, const float dt) {
	crabnado.mode_time += dt;
	const float t = std::min(1.0F, crabnado.mode_time / duration);
	crabnado.radius = crabnado.leg_from + (to - crabnado.leg_from) * t;
	return t >= 1.0F;
}

} // namespace

glm::vec2 OrbitPoint(const float bearing, const float radius) {
	return {std::cos(bearing) * radius, std::sin(bearing) * radius};
}

float BearingOf(const glm::vec2 point) {
	return std::atan2(point.y, point.x);
}

float PickOrbitBearing(const flecs::world& world) {
	constexpr float tau = 2.0F * std::numbers::pi_v<float>;
	auto& rng = world.get_mut<ecs::RngState>();
	const flecs::entity player = FindPlayer(world);
	if (player) {
		const glm::vec3 p = player.get<spatial::Transform>().position;
		if (glm::length(glm::vec2{p.x, p.z}) > 1.0F) {
			// Anywhere on the half of the circle facing away from the player.
			const float away = BearingOf({p.x, p.z}) + std::numbers::pi_v<float>;
			const float spread = std::numbers::pi_v<float> - constants::kOrbitStartMinAngle;
			return std::remainder(away + rng.NextFloatRange(-spread, spread), tau);
		}
	}
	return rng.NextFloatRange(0.0F, tau);
}

void RegisterCrabnado(const flecs::world& world) {
	world.component<CrabnadoMode>()
		.constant("Orbit", CrabnadoMode::Orbit)
		.constant("Breach", CrabnadoMode::Breach)
		.constant("Fight", CrabnadoMode::Fight)
		.constant("Retreat", CrabnadoMode::Retreat);
	world.component<Crabnado>()
		.member<float>("bearing")
		.member<int>("direction")
		.member<float>("radius")
		.member<CrabnadoMode>("mode")
		.member<float>("mode_time")
		.member<float>("leg_from")
		.member<int>("shifts_seen")
		.member<float>("spawn_debt")
		.member<glm::vec2>("knock");
	world.component<ThrowsInto>().add(flecs::Exclusive);
	world.component<FunnelRing>().member<float>("lift").member<float>("time");
	world.component<FunnelStorm>()
		.member<float>("storm")
		.member<float>("flash")
		.member<int>("slams_seen")
		.member<core::Rgba>("storm_color");
	world.component<FunnelTint>().member<core::Rgba>("base");

	// The spin lives here, not in CrabnadoOrbit, because it speeds up with the storm.
	world.system<Crabnado, FunnelStorm, spatial::Transform>("CrabnadoStorm")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, Crabnado& crabnado, FunnelStorm& storm, spatial::Transform& transform) {
			const float dt = it.delta_time();
			const auto& run = it.world().get<RunState>();
			if (const BossDef* boss = CurrentBoss(it.world())) {
				storm.storm_color = boss->slam.storm_color;
			}
			// The storm calms as the beaten crabnado is blown back out.
			storm.storm = crabnado.mode == CrabnadoMode::Retreat ? std::max(0.0F, storm.storm - dt / constants::kRetreatTime)
																  : StormLevel(it.world());
			if (run.slams > storm.slams_seen) {
				storm.slams_seen = run.slams;
				storm.flash = 1.0F;
			}
			storm.flash = std::max(0.0F, storm.flash - constants::kStormFlashFade * dt);
			transform.rotation.y += kSpin * (1.0F + (constants::kStormSpin - 1.0F) * storm.storm) * dt;
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// FunnelStorm sits on the crabnado, two levels up (crabnado -> ring -> part).
	world.system<const FunnelTint, render::Material, const FunnelStorm>("FunnelTint")
		.term_at(2)
		.up()
		.kind(flecs::OnUpdate)
		.each([](const FunnelTint& tint, render::Material& material, const FunnelStorm& storm) {
			const float dark = storm.storm * (1.0F - storm.flash);
			const auto mix = [dark](const uint8_t from, const uint8_t to) {
				return static_cast<uint8_t>(static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * dark);
			};
			material.color = {.r = mix(tint.base.r, storm.storm_color.r),
				.g = mix(tint.base.g, storm.storm_color.g),
				.b = mix(tint.base.b, storm.storm_color.b),
				.a = 255};
		})
		.add<scene::GameScene>();

	// Not Pausable: the title has no pause, and the funnel can keep churning behind the molt modal.
	world.system<FunnelRing, spatial::Transform>("FunnelSway")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, FunnelRing& ring, spatial::Transform& transform) {
			// The arena crabnado's storm makes the sway faster and wider; the title's has none.
			const auto* storm = it.entity(i).parent().try_get<FunnelStorm>();
			const float level = storm ? storm->storm : 0.0F;
			ring.time += it.delta_time() * (1.0F + (constants::kStormSwayRate - 1.0F) * level);
			const float phase = ring.time * kSwayRate - ring.lift * kSwayLag;
			const float reach = kSwayReach * ring.lift * (1.0F + (constants::kStormSwayReach - 1.0F) * level);
			transform.position.x = reach * std::sin(phase);
			transform.position.z = reach * 0.6F * std::cos(phase);
			transform.rotation.y = kSwayTwist * std::sin(phase);
		})
		.add<scene::GameScene>();

	// Orbit, and the Breach and Retreat legs either side of the fight (the Fight itself is boss.cpp's). It also tells the
	// barrier where the crabnado is, so the pieces beside it flare.
	world.system<Crabnado, spatial::Transform>("CrabnadoOrbit")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, Crabnado& crabnado, spatial::Transform& transform) {
			const flecs::world world = it.world();
			const float dt = it.delta_time();
			const auto& run = world.get<RunState>();
			const auto& wind = world.get<Wind>();
			if (wind.shifts != crabnado.shifts_seen) {
				crabnado.shifts_seen = wind.shifts;
				crabnado.direction = -crabnado.direction;
			}
			const BossDef* boss = CurrentBoss(world);
			if (boss == nullptr) {
				return;
			}
			const SlamTunables& tune = boss->slam;
			// A jump back to a normal Stage ends the fight where it stands.
			if (!InBossStage(world) && crabnado.mode != CrabnadoMode::Orbit) {
				crabnado.mode = CrabnadoMode::Orbit;
			}
			if (crabnado.mode == CrabnadoMode::Orbit && InBossStage(world) && run.phase == RunPhase::Boss) {
				crabnado.mode = CrabnadoMode::Breach;
				crabnado.mode_time = 0.0F;
				crabnado.leg_from = crabnado.radius;
			}

			float flare = 0.0F;
			switch (crabnado.mode) {
			case CrabnadoMode::Orbit: StepOrbit(world, crabnado, tune, dt); break;
			case CrabnadoMode::Breach:
				flare = 1.0F;
				if (StepLeg(crabnado, constants::kArenaRadius - tune.edge_margin, constants::kBreachTime, dt)) {
					crabnado.mode = CrabnadoMode::Fight;
				}
				break;
			case CrabnadoMode::Fight: flare = 1.0F; break;
			case CrabnadoMode::Retreat:
				flare = 1.0F - std::min(1.0F, crabnado.mode_time / constants::kRetreatTime);
				if (StepLeg(crabnado, tune.orbit_radius, constants::kRetreatTime, dt)) {
					crabnado.mode = CrabnadoMode::Orbit;
					crabnado.knock = glm::vec2{0.0F};
					// The column is over the barrier again: the Act is done.
					ClearAct(world);
				}
				break;
			}
			if (crabnado.mode != CrabnadoMode::Fight) {
				const glm::vec2 at = OrbitPoint(crabnado.bearing, crabnado.radius);
				transform.position.x = at.x;
				transform.position.z = at.y;
			}
			auto& barrier = world.get_mut<BarrierFlare>();
			barrier.bearing = BearingOf({transform.position.x, transform.position.z});
			barrier.strength = flare;
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// The spawn rate is the current Stage's curve on the Stage clock (the Boss Stage's is fixed).
	world.system<Crabnado, const spatial::Transform>("CrabnadoThrow")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, Crabnado& crabnado, const spatial::Transform& transform) {
			const flecs::world world = it.world();
			const auto& run = world.get<RunState>();
			const auto plan = CurrentSpawnPlan(world);
			// No new throws while the storm rises, and none from a crabnado that isn't throwing: breaching or retreating.
			const bool throwing = run.phase != RunPhase::Boss || crabnado.mode == CrabnadoMode::Fight;
			if (!IsRunning(run.phase) || run.act_clock < constants::kFirstThrowDelay || run.storm_up > 0.0F || !throwing || !plan) {
				return;
			}
			crabnado.spawn_debt += plan->rate * it.delta_time();
			const int throws = static_cast<int>(crabnado.spawn_debt);
			crabnado.spawn_debt -= static_cast<float>(throws);
			ThrowAtPlayer(world, it.entity(i), transform.position, throws);
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();
}

void AddFunnel(const flecs::world& world, const flecs::entity crabnado) {
	constexpr float tau = 2.0F * std::numbers::pi_v<float>;
	auto& rng = world.get_mut<ecs::RngState>();
	// Everything hangs from its ring's pivot, positioned relative to it, so it sways with that ring.
	std::array<flecs::entity, kFunnelRings> pivots;
	for (int ring = 0; ring < kFunnelRings; ++ring) {
		const float radius = kFunnelBaseRadius + kFunnelFlare * static_cast<float>(ring);
		const float y = RingHeight(ring);
		const flecs::entity pivot =
			world.entity()
				.child_of(crabnado)
				.set<FunnelRing>({.lift = static_cast<float>(ring) / static_cast<float>(kFunnelRings - 1)})
				.set<spatial::Transform>({.position = {0.0F, y, 0.0F}});
		pivots[static_cast<size_t>(ring)] = pivot;
		const float slice = tau / kWispsPerRing;
		// Half the strand's length, along the ring.
		const float half = std::max(radius * slice * kWispArc / 2.0F, kWispMinHalfLength);
		for (int i = 0; i < kWispsPerRing; ++i) {
			const float angle = kFunnelTwist * static_cast<float>(ring) + slice * static_cast<float>(i) +
				rng.NextFloatRange(-0.2F, 0.2F) * slice;
			const float r = radius + rng.NextFloatRange(-kWispJitter, kWispJitter);
			const float shade = rng.NextFloat();
			const core::Rgba color{
				.r = static_cast<uint8_t>(kFunnelColor.r + (kWispColor.r - kFunnelColor.r) * shade),
				.g = static_cast<uint8_t>(kFunnelColor.g + (kWispColor.g - kFunnelColor.g) * shade),
				.b = static_cast<uint8_t>(kFunnelColor.b + (kWispColor.b - kFunnelColor.b) * shade),
				.a = 255,
			};
			const float span = 2.0F * half * rng.NextFloatRange(0.8F, 1.2F) / r; // radians of arc
			const float step = span / kWispSegments;
			const float base = rng.NextFloatRange(-kWispJitter, kWispJitter);
			for (int segment = 0; segment < kWispSegments; ++segment) {
				const float t = (static_cast<float>(segment) + 0.5F) / kWispSegments;
				const float a = angle + span * (t - 0.5F);
				const float thickness =
					kWispTubeRadius * (kWispTaper + (1.0F - kWispTaper) * std::sin(std::numbers::pi_v<float> * t));
				// A unit sphere stretched along its local x, yawed so x runs along the ring's tangent at `a`.
				std::ignore = world.entity()
								  .child_of(pivot)
								  .set<spatial::Transform>(
									  {.position = {std::cos(a) * r, base + kWispRise * (t - 0.5F), std::sin(a) * r},
									   .rotation = {0.0F, -a - std::numbers::pi_v<float> / 2.0F, 0.0F},
									   .scale = {r * step / 2.0F * kWispOverlap, thickness, thickness}}
								  )
								  .set<render::SpherePrimitive>({.radius = 1.0F})
								  .set<FunnelTint>({.base = color})
								  .set<render::Material>({.color = color, .cast_shadow = false});
			}
		}
	}
	const float height = kFunnelRingHeight * static_cast<float>(kFunnelRings);
	for (int i = 0; i < kPuffs; ++i) {
		const float h = rng.NextFloat();
		const float angle = rng.NextFloatRange(0.0F, 2.0F * std::numbers::pi_v<float>);
		const float wall = kFunnelBaseRadius + kFunnelFlare * static_cast<float>(kFunnelRings) * h;
		// sqrt spreads the puffs evenly over the funnel's cross-section instead of bunching them at its axis.
		const float radius = wall * kPuffReach * std::sqrt(rng.NextFloat());
		const float size = kPuffMinRadius + (kPuffMaxRadius - kPuffMinRadius) * h * rng.NextFloatRange(0.6F, 1.0F);
		const int ring = NearestRing(h * height);
		const flecs::entity pivot = pivots[static_cast<size_t>(ring)];
		const glm::vec3 center{std::cos(angle) * radius, h * height - RingHeight(ring), std::sin(angle) * radius};
		for (int blob = 0; blob < kPuffBlobs; ++blob) {
			const glm::vec3 offset{rng.NextFloatRange(-0.7F, 0.7F), rng.NextFloatRange(-0.35F, 0.35F),
				rng.NextFloatRange(-0.7F, 0.7F)};
			std::ignore = world.entity()
							  .child_of(pivot)
							  .set<spatial::Transform>(
								  {.position = center + offset * size, .scale = {1.0F, rng.NextFloatRange(0.6F, 0.85F), 1.0F}}
							  )
							  .set<render::SpherePrimitive>({.radius = size * rng.NextFloatRange(0.6F, 1.0F)})
							  .set<FunnelTint>({.base = kPuffColor})
							  .set<render::Material>({.color = kPuffColor, .cast_shadow = false});
		}
	}
	for (int i = 0; i < kSpecks; ++i) {
		const float h = rng.NextFloat();
		const float angle = rng.NextFloatRange(0.0F, 2.0F * std::numbers::pi_v<float>);
		// Just outside the funnel wall at that height.
		const float radius = kFunnelBaseRadius + kFunnelFlare * static_cast<float>(kFunnelRings) * h + 0.6F;
		const int ring = NearestRing(h * height);
		const flecs::entity pivot = pivots[static_cast<size_t>(ring)];
		const float y = h * height - RingHeight(ring);
		std::ignore = world.entity()
						  .child_of(pivot)
						  .set<spatial::Transform>(
							  {.position = {std::cos(angle) * radius, y, std::sin(angle) * radius}}
						  )
						  .set<render::SpherePrimitive>({.radius = 0.45F})
						  .set<render::Material>({.color = kSpeckColor});
	}
}

flecs::entity SpawnCrabnado(const flecs::world& world, const flecs::entity parent, const flecs::entity horde) {
	const float bearing = PickOrbitBearing(world);
	const glm::vec2 position = OrbitPoint(bearing, constants::kOrbitFallbackRadius);
	const flecs::entity crabnado =
		world.entity("Crabnado")
			.child_of(parent)
			.set<Crabnado>({.bearing = bearing, .radius = constants::kOrbitFallbackRadius, .shifts_seen = world.get<Wind>().shifts})
			.add<ThrowsInto>(horde)
			.set<FunnelStorm>({})
			.set<spatial::Transform>({.position = {position.x, 0.0F, position.y}});

	AddFunnel(world, crabnado);
	return crabnado;
}

void ThrowBurst(const flecs::entity crabnado, const int count) {
	ThrowAtPlayer(crabnado.world(), crabnado, crabnado.get<spatial::Transform>().position, count);
}

} // namespace crabnado
