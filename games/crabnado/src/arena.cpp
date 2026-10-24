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
constexpr core::Rgba kPoolColor{.r = 0x1d, .g = 0x66, .b = 0x80, .a = 255};
constexpr core::Rgba kRockColor{.r = 0x6f, .g = 0x7c, .b = 0x80, .a = 255};
constexpr core::Rgba kDriftwoodColor{.r = 0x7a, .g = 0x66, .b = 0x50, .a = 255};
// A rock is a sphere flattened to this fraction of its radius, sunk so its top sits at kRockHeight radii.
constexpr float kRockFlatten = 0.7F;
constexpr float kRockLift = 0.3F;
// Driftwood boxes sit this far outside the shore, kDriftwoodMinLength–kDriftwoodMaxLength long.
constexpr float kDriftwoodOffset = 3.0F;
constexpr float kDriftwoodMinLength = 2.0F;
constexpr float kDriftwoodMaxLength = 4.0F;
constexpr float kDriftwoodGirth = 0.7F;

void DressRock(const flecs::world& world, const flecs::entity rock, const float radius) {
	rock.get_mut<spatial::Transform>().position.y = radius * kRockLift;
	std::ignore = world.entity()
					  .child_of(rock)
					  .set<spatial::Transform>({.scale = {1.0F, kRockFlatten, 1.0F}})
					  .set<render::SpherePrimitive>({.radius = radius})
					  .set<render::Material>({.color = kRockColor});
}

void SpawnDriftwood(const flecs::world& world, const flecs::entity parent, const ArenaEdge& edge) {
	const flecs::entity ring = world.entity("Driftwood").child_of(parent);
	for (int i = 0; i < edge.driftwood; ++i) {
		const float angle = static_cast<float>(i) / static_cast<float>(edge.driftwood) * 2.0F * std::numbers::pi_v<float>;
		// A fixed pseudo-random length per box, so the ring looks the same every run.
		const float length = kDriftwoodMinLength +
							 (kDriftwoodMaxLength - kDriftwoodMinLength) * (0.5F + 0.5F * std::sin(static_cast<float>(i) * 12.9898F));
		const float radius = edge.radius + kDriftwoodOffset;
		std::ignore = world.entity()
						  .child_of(ring)
						  .set<spatial::Transform>(
							  {.position = {std::cos(angle) * radius, 0.0F, std::sin(angle) * radius},
							   .rotation = {0.0F, -angle, 0.0F}}
						  )
						  .set<render::CubePrimitive>({.size = {kDriftwoodGirth, kDriftwoodGirth, length}})
						  .set<render::Material>({.color = kDriftwoodColor});
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
	// The water tiles were spawned before the pools were known: tint the ones inside a pool.
	world.query_builder<WaterTile, const spatial::Transform>().build().each(
		[&](WaterTile& tile, const spatial::Transform& transform) {
			const glm::vec2 center{transform.position.x, transform.position.z};
			if (std::ranges::any_of(pools, [&](const TidePool& pool) { return glm::length(center - pool.center) < pool.radius; })) {
				tile.base = kPoolColor;
			}
		}
	);

	SpawnDriftwood(world, parent, world.get<ArenaEdge>());
	spdlog::info("[Arena] {} rocks, {} pools", rocks.size(), pools.size());
}

} // namespace

void RegisterArena(const flecs::world& world) {
	world.component<Rock>().member<float>("radius");
	world.component<PoolSite>().member<float>("radius");
	world.component<ArenaEdge>().member<float>("radius").member<int>("driftwood");
	world.set<ArenaEdge>({.radius = constants::kArenaRadius});
	world.set<Rocks>({});
	world.set<TidePools>({});

	world.set<ArenaPending>({});

	world.system<ArenaPending>("ArenaBuild")
		.term_at(0)
		.singleton()
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, ArenaPending& pending) {
			if (!pending.parent) {
				return;
			}
			CacheAndDress(it.world(), pending.parent);
			pending.parent = {};
		})
		.add<scene::GameScene>();

	level::RegisterComponentLoader(world, "rock", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<Rock>({.radius = j.value("radius", 1.0F)});
	});
	level::RegisterComponentLoader(world, "tide_pool", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<PoolSite>({.radius = j.value("radius", 1.0F)});
	});
	level::RegisterSingletonLoader(world, "arena_edge", [](const flecs::world& w, const nlohmann::json& j) {
		w.set<ArenaEdge>({.radius = j.value("radius", constants::kArenaRadius), .driftwood = j.value("driftwood", 0)});
	});
}

void BuildArena(const flecs::world& world, const flecs::entity parent) {
	if (level::LoadLevel(world, assets::ResolveAsset(world, kArenaLevel), parent) < 0) {
		spdlog::error("[Arena] Could not load {}", kArenaLevel);
		return;
	}
	world.set<ArenaPending>({.parent = parent});
}

void ClearArena(const flecs::world& world) {
	world.set<ArenaPending>({});
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
