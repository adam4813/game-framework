#include "arena.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr const char* kArenaLevel = "levels/crabnado/arena.level.json";
constexpr core::Rgba kRockColor{.r = 0x6f, .g = 0x7c, .b = 0x80, .a = 255};
// A rock is a sphere flattened to this fraction of its radius, sunk so its top sits at kRockHeight radii.
constexpr float kRockFlatten = 0.7F;
constexpr float kRockLift = 0.3F;
// Tries per scattered rock before it is given up (the band is too crowded).
constexpr int kScatterAttempts = 40;

// The barrier: a hovering ward line of glowing segments kWardOffset outside the arena edge, with dark rune pillars
// standing on it, each banded with a glowing rune and topped by a floating orb that sheds sparkles.
constexpr float kWardOffset = 1.5F;
constexpr int kWardSegments = 120;
constexpr float kWardHeight = 1.8F;
constexpr float kWardThickness = 0.5F;
constexpr glm::vec3 kPillarSize{1.3F, 5.5F, 1.3F};
constexpr float kPillarY = 1.6F;
constexpr glm::vec3 kRuneBandSize{1.5F, 0.5F, 1.5F};
constexpr float kRuneBandY = 2.8F;
constexpr float kOrbRadius = 0.8F;
constexpr float kOrbY = 5.4F;
constexpr core::Rgba kPillarColor{.r = 0x2c, .g = 0x2a, .b = 0x44, .a = 255};
constexpr core::Rgba kWardViolet{.r = 0x9b, .g = 0x5c, .b = 0xff, .a = 255};
constexpr core::Rgba kWardCyan{.r = 0x5c, .g = 0xf0, .b = 0xff, .a = 255};
constexpr core::Rgba kWardFlash{.r = 0xf4, .g = 0xf0, .b = 0xff, .a = 255};
constexpr core::Rgba kSparkStart{.r = 0xb8, .g = 0xf6, .b = 0xff, .a = 230};
constexpr core::Rgba kSparkEnd{.r = 0x9b, .g = 0x5c, .b = 0xff, .a = 0};
// The shimmer: colour bands kShimmerBands times round the ring drifting at kShimmerSpeed, a bright flash circling
// at kFlashSpeed radians per second (kFlashSharpness narrows it), and a bob of kWardBob (segments) or kOrbBob (orbs);
// rune bands stay put on their pillars.
constexpr float kShimmerBands = 3.0F;
constexpr float kShimmerSpeed = 1.6F;
constexpr float kFlashSpeed = 0.7F;
constexpr float kFlashSharpness = 24.0F;
constexpr float kWardBob = 0.15F;
constexpr float kOrbBob = 0.3F;

core::Rgba Lerp(const core::Rgba a, const core::Rgba b, const float t) {
	const auto channel = [t](const uint8_t from, const uint8_t to) {
		return static_cast<uint8_t>(std::lround(static_cast<float>(from) + (static_cast<float>(to) - from) * t));
	};
	return {.r = channel(a.r, b.r), .g = channel(a.g, b.g), .b = channel(a.b, b.b), .a = 255};
}

void DressRock(const flecs::world& world, const flecs::entity rock, const float radius) {
	rock.get_mut<spatial::Transform>().position.y = radius * kRockLift;
	std::ignore = world.entity()
					  .child_of(rock)
					  .set<spatial::Transform>({.scale = {1.0F, kRockFlatten, 1.0F}})
					  .set<render::SpherePrimitive>({.radius = radius})
					  .set<render::Material>({.color = kRockColor});
}

RockBand ParseBand(const nlohmann::json& j) {
	return {
		.count = j.value("count", 0),
		.min_distance = j.value("min_distance", 0.0F),
		.max_distance = j.value("max_distance", 0.0F),
		.min_radius = j.value("min_radius", 1.0F),
		.max_radius = j.value("max_radius", 1.0F)};
}

// Rolls the RockScatter bands into rock entities under `parent`, clear of the pools, the level's own rocks,
// `keepClear` and each other.
void ScatterRocks(const flecs::world& world, const flecs::entity parent, const std::span<const RockCircle> keepClear) {
	const auto& scatter = world.get<RockScatter>();
	std::vector<RockCircle> taken(keepClear.begin(), keepClear.end());
	world.query_builder<const PoolSite, const spatial::Transform>().build().each(
		[&](const PoolSite& pool, const spatial::Transform& transform) {
			taken.push_back({.center = {transform.position.x, transform.position.z}, .radius = pool.radius});
		}
	);
	world.query_builder<const Rock, const spatial::Transform>().build().each(
		[&](const Rock& rock, const spatial::Transform& transform) {
			taken.push_back({.center = {transform.position.x, transform.position.z}, .radius = rock.radius});
		}
	);

	auto& rng = world.get_mut<ecs::RngState>();
	const flecs::entity group = world.entity("ScatteredRocks").child_of(parent);
	int wanted = 0;
	int placed = 0;
	for (const RockBand* band : {&scatter.inner, &scatter.outer}) {
		wanted += band->count;
		for (int i = 0; i < band->count; ++i) {
			for (int attempt = 0; attempt < kScatterAttempts; ++attempt) {
				const float radius = rng.NextFloatRange(band->min_radius, band->max_radius);
				const float distance = rng.NextFloatRange(band->min_distance, band->max_distance);
				const float angle = rng.NextFloatRange(0.0F, 2.0F * std::numbers::pi_v<float>);
				const glm::vec2 center{std::cos(angle) * distance, std::sin(angle) * distance};
				const bool blocked = std::ranges::any_of(taken, [&](const RockCircle& other) {
					return glm::length(center - other.center) < other.radius + radius + scatter.gap;
				});
				if (blocked) {
					continue;
				}
				taken.push_back({.center = center, .radius = radius});
				std::ignore = world.entity()
								  .child_of(group)
								  .set<spatial::Transform>({.position = {center.x, 0.0F, center.y}})
								  .set<Rock>({.radius = radius});
				++placed;
				break;
			}
		}
	}
	if (placed < wanted) {
		spdlog::warn("[Arena] Scattered {} of {} rocks; the rock_scatter bands are too crowded", placed, wanted);
	}
}

// A piece of the barrier: an unlit, glowing primitive that BarrierShimmer recolours and bobs.
flecs::entity AddGlow(
	const flecs::world& world, const flecs::entity parent, const float angle, const glm::vec3 position, const float bob
) {
	return world.entity()
		.child_of(parent)
		.set<spatial::Transform>({.position = position, .rotation = {0.0F, -angle, 0.0F}})
		.set<render::Material>({.color = kWardViolet, .cast_shadow = false, .unlit = true})
		.set<BarrierGlow>({.angle = angle, .rest_y = position.y, .bob = bob});
}

void SpawnBarrier(const flecs::world& world, const flecs::entity parent, const ArenaEdge& edge) {
	const flecs::entity barrier = world.entity("Barrier").child_of(parent);
	const float radius = edge.radius + kWardOffset;
	const float tau = 2.0F * std::numbers::pi_v<float>;
	// Each segment is a touch longer than its share of the circle, so the ward line has no gaps.
	const float segmentLength = tau * radius / static_cast<float>(kWardSegments) * 1.05F;
	for (int i = 0; i < kWardSegments; ++i) {
		const float angle = static_cast<float>(i) / static_cast<float>(kWardSegments) * tau;
		std::ignore =
			AddGlow(world, barrier, angle, {std::cos(angle) * radius, kWardHeight, std::sin(angle) * radius}, kWardBob)
				.set<render::CubePrimitive>({.size = {kWardThickness, kWardThickness, segmentLength}});
	}
	for (int i = 0; i < edge.pillars; ++i) {
		const float angle = (static_cast<float>(i) + 0.5F) / static_cast<float>(edge.pillars) * tau;
		const glm::vec2 at{std::cos(angle) * radius, std::sin(angle) * radius};
		std::ignore = world.entity()
						  .child_of(barrier)
						  .set<spatial::Transform>({.position = {at.x, kPillarY, at.y}, .rotation = {0.0F, -angle, 0.0F}})
						  .set<render::CubePrimitive>({.size = kPillarSize})
						  .set<render::Material>({.color = kPillarColor});
		std::ignore = AddGlow(world, barrier, angle, {at.x, kRuneBandY, at.y}, 0.0F).set<render::CubePrimitive>({.size = kRuneBandSize});
		const flecs::entity orb =
			AddGlow(world, barrier, angle, {at.x, kOrbY, at.y}, kOrbBob).set<render::SpherePrimitive>({.radius = kOrbRadius});
		std::ignore = world.entity()
						  .child_of(orb)
						  .set<spatial::Transform>({})
						  .set<particles::ParticleEmitter>(
							  {.rate = 10.0F,
							   .particleLifetime = 1.4F,
							   .speed = 1.6F,
							   .startSize = 0.2F,
							   .spread = 0.9F,
							   .colorStart = kSparkStart,
							   .colorEnd = kSparkEnd}
						  );
	}
}

void CacheAndDress(const flecs::world& world, const flecs::entity parent) {
	auto& rocks = world.get_mut<Rocks>().circles;
	rocks.clear();
	std::vector<flecs::entity> rockEntities;
	world.query_builder<const Rock, const spatial::Transform>().build().each(
		[&](const flecs::entity e, const Rock& rock, const spatial::Transform& transform) {
			rocks.push_back({.center = {transform.position.x, transform.position.z}, .radius = rock.radius});
			rockEntities.push_back(e);
		}
	);
	// Dress after the query, so no entity is added to a rock mid-iteration.
	for (const flecs::entity rock : rockEntities) {
		DressRock(world, rock, rock.get<Rock>().radius);
	}

	auto& pools = world.get_mut<TidePools>().pools;
	pools.clear();
	world.query_builder<const PoolSite, const spatial::Transform>().build().each(
		[&](const PoolSite& pool, const spatial::Transform& transform) {
			pools.push_back({.center = {transform.position.x, transform.position.z}, .radius = pool.radius});
		}
	);
	SpawnBarrier(world, parent, world.get<ArenaEdge>());
	spdlog::info("[Arena] {} rocks, {} pools", rocks.size(), pools.size());
}

} // namespace

void RegisterArena(const flecs::world& world) {
	world.component<Rock>().member<float>("radius");
	world.component<PoolSite>().member<float>("radius");
	world.component<ArenaEdge>().member<float>("radius").member<int>("pillars");
	world.component<RockBand>()
		.member<int>("count")
		.member<float>("min_distance")
		.member<float>("max_distance")
		.member<float>("min_radius")
		.member<float>("max_radius");
	world.component<RockScatter>().member<RockBand>("inner").member<RockBand>("outer").member<float>("gap");
	world.component<BarrierGlow>().member<float>("angle").member<float>("rest_y").member<float>("bob");
	world.set<RockScatter>({});
	world.set<ArenaEdge>({.radius = constants::kArenaRadius});
	world.set<Rocks>({});
	world.set<TidePools>({});

	level::RegisterComponentLoader(world, "rock", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<Rock>({.radius = j.value("radius", 1.0F)});
	});
	level::RegisterComponentLoader(world, "tide_pool", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<PoolSite>({.radius = j.value("radius", 1.0F)});
	});
	level::RegisterSingletonLoader(world, "arena_edge", [](const flecs::world& w, const nlohmann::json& j) {
		w.set<ArenaEdge>({.radius = j.value("radius", constants::kArenaRadius), .pillars = j.value("pillars", 0)});
	});
	level::RegisterSingletonLoader(world, "rock_scatter", [](const flecs::world& w, const nlohmann::json& j) {
		w.set<RockScatter>(
			{.inner = ParseBand(j.value("inner", nlohmann::json::object())),
			 .outer = ParseBand(j.value("outer", nlohmann::json::object())),
			 .gap = j.value("gap", 0.0F)}
		);
	});

	// The barrier's colour bands and a bright flash circle the ring while each piece bobs. Not pausable, so the
	// barrier keeps shimmering behind the pause menu.
	world.system<const BarrierGlow, spatial::Transform, render::Material>("BarrierShimmer")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, const BarrierGlow& glow, spatial::Transform& transform,
				 render::Material& material) {
			const auto time = static_cast<float>(it.world().get_info()->world_time_total);
			const float band = 0.5F + 0.5F * std::sin(glow.angle * kShimmerBands - time * kShimmerSpeed);
			const float flash = std::pow(std::max(0.0F, std::cos(glow.angle - time * kFlashSpeed)), kFlashSharpness);
			material.color = Lerp(Lerp(kWardViolet, kWardCyan, band), kWardFlash, flash);
			transform.position.y = glow.rest_y + glow.bob * std::sin(time * 2.0F + glow.angle * 6.0F);
		})
		.add<scene::GameScene>();
}

void BuildArena(const flecs::world& world, const flecs::entity parent, const std::span<const RockCircle> keepClear) {
	if (level::LoadLevel(world, assets::ResolveAsset(world, kArenaLevel), parent) < 0) {
		spdlog::error("[Arena] Could not load {}", kArenaLevel);
		return;
	}
	ScatterRocks(world, parent, keepClear);
	CacheAndDress(world, parent);
}

void ClearArena(const flecs::world& world) {
	world.set<Rocks>({});
	world.set<TidePools>({});
}

bool PushOutOfRocks(const std::span<const RockCircle> rocks, glm::vec2& position, const float radius) {
	for (const RockCircle& rock : rocks) {
		const glm::vec2 apart = position - rock.center;
		const float distance = glm::length(apart);
		const float touching = rock.radius + radius;
		if (distance < touching) {
			// Dead centre has no direction to push along; any one will do.
			position = rock.center + (distance > 0.0001F ? apart / distance : glm::vec2{1.0F, 0.0F}) * touching;
			return true;
		}
	}
	return false;
}

} // namespace crabnado
