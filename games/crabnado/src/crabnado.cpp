#include "crabnado.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

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
// The colour the storm darkens the funnel toward.
constexpr core::Rgba kStormColor{.r = 62, .g = 70, .b = 86, .a = 255};
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

// Throws up to `count` crabs from the crabnado toward the player, stopping at the horde cap.
void ThrowAtPlayer(const flecs::world& world, const flecs::entity crabnado, const glm::vec3 position, int count) {
	count = std::min(count, constants::kHordeCap - world.count<Enemy>());
	const flecs::entity horde = crabnado.target<ThrowsInto>();
	if (count <= 0 || !horde) {
		return;
	}
	const glm::vec2 from{position.x, position.z};
	glm::vec2 target = from;
	if (const flecs::entity player = FindPlayer(world)) {
		const glm::vec3 p = player.get<spatial::Transform>().position;
		target = {p.x, p.z};
	}
	for (int i = 0; i < count; ++i) {
		const int type = PickEnemyType(world, world.get<RunState>().clock);
		if (type >= 0) {
			ThrowEnemy(world, horde, type, from, target);
		}
	}
}

// A random point in the roaming disc, shifted downwind and held inside the disc, so the wind steers the
// crabnado without sending it to the arena edge.
glm::vec2 PickWaypoint(const flecs::world& world) {
	auto& rng = world.get_mut<ecs::RngState>();
	const float roam = constants::kArenaRadius * constants::kCrabnadoRoamFraction;
	const float angle = rng.NextFloatRange(0.0F, 2.0F * std::numbers::pi_v<float>);
	// sqrt spreads waypoints evenly over the disc instead of bunching them at the centre.
	const float radius = roam * std::sqrt(rng.NextFloat());
	const glm::vec2 waypoint =
		glm::vec2{std::cos(angle), std::sin(angle)} * radius + WindPush(world.get<Wind>(), constants::kCrabnadoWindBias);
	const float distance = glm::length(waypoint);
	return distance > roam ? waypoint * (roam / distance) : waypoint;
}

} // namespace

void RegisterCrabnado(const flecs::world& world) {
	world.component<Crabnado>()
		.member<glm::vec2>("waypoint")
		.member<float>("speed")
		.member<float>("spawn_debt")
		.member<glm::vec2>("knock");
	world.component<ThrowsInto>().add(flecs::Exclusive);
	world.component<FunnelRing>().member<float>("lift").member<float>("time");
	world.component<FunnelStorm>().member<float>("storm").member<float>("flash").member<int>("slams_seen");
	world.component<FunnelTint>().member<core::Rgba>("base");

	// The spin lives here, not in CrabnadoRoam, because it speeds up with the storm.
	world.system<Crabnado, FunnelStorm, spatial::Transform>("CrabnadoStorm")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, Crabnado&, FunnelStorm& storm, spatial::Transform& transform) {
			const float dt = it.delta_time();
			const auto& run = it.world().get<RunState>();
			storm.storm = std::clamp(
				(run.clock - constants::kBossWindUpAt) / (constants::kBossAt - constants::kBossWindUpAt), 0.0F, 1.0F
			);
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
			material.color = {.r = mix(tint.base.r, kStormColor.r),
				.g = mix(tint.base.g, kStormColor.g),
				.b = mix(tint.base.b, kStormColor.b),
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

	world.system<Crabnado, spatial::Transform>("CrabnadoRoam")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, Crabnado& crabnado, spatial::Transform& transform) {
			const float dt = it.delta_time();
			// The boss phase moves it instead (see boss.cpp).
			if (it.world().get<RunState>().phase >= RunPhase::Boss) {
				return;
			}
			const glm::vec2 position{transform.position.x, transform.position.z};
			const glm::vec2 toWaypoint = crabnado.waypoint - position;
			const float distance = glm::length(toWaypoint);
			if (distance <= constants::kCrabnadoArriveRadius) {
				crabnado.waypoint = PickWaypoint(it.world());
				return;
			}
			const glm::vec2 step = toWaypoint / distance * std::min(crabnado.speed * dt, distance);
			transform.position.x += step.x;
			transform.position.z += step.y;
		})
		.add<ecs::Pausable>()
		.add<scene::GameScene>();

	// The spawn rate rises linearly with the run clock.
	world.system<Crabnado, const spatial::Transform>("CrabnadoThrow")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, Crabnado& crabnado, const spatial::Transform& transform) {
			const flecs::world world = it.world();
			const auto& run = world.get<RunState>();
			if (!IsRunning(run.phase) || run.clock < constants::kFirstThrowDelay) {
				return;
			}
			const float factor = run.phase == RunPhase::Boss ? constants::kBossSpawnFactor : 1.0F;
			crabnado.spawn_debt +=
				(constants::kSpawnRateBase + constants::kSpawnRateGrowth * run.clock) * factor * it.delta_time();
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

flecs::entity SpawnCrabnado(
	const flecs::world& world, const flecs::entity parent, const flecs::entity horde, const glm::vec2 position
) {
	const flecs::entity crabnado = world.entity("Crabnado")
									  .child_of(parent)
									  .set<Crabnado>({.waypoint = PickWaypoint(world), .speed = constants::kCrabnadoSpeed})
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
