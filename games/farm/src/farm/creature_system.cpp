#include "creature_system.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>


#include "creature_components.hpp"
#include "engine/engine.hpp"
#include "upright_object_mesh_builder.hpp"

using namespace engine;

namespace game::farm {
namespace {

// World-unit size of one tile for the grid → world placement (tile_size_px 16 / ppu 32).
constexpr float kStep = 0.5f;
constexpr float kCreatureWidth = 0.4f; // world units (~0.8 tile wide)

// Eight-connected step directions used by the wander action.
constexpr glm::ivec2 kDirs[8] = {
	{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1},
};

// Self-contained splitmix32 so each creature carries its own deterministic wander stream — no shared
// RNG singleton (which would be unsafe to lazily create inside the readonly BT system).
std::uint32_t NextRng(std::uint32_t& s) {
	s += 0x9E3779B9u;
	std::uint32_t z = s;
	z = (z ^ (z >> 16)) * 0x21F0AAADu;
	z = (z ^ (z >> 15)) * 0x735A2D97u;
	return z ^ (z >> 15);
}

ai::BehaviorTree MakeCreatureBtTree() {
	ai::BehaviorTree bt;
	const auto root = bt.tree.AddNode({.type = ai::BTNodeType::Repeater, .repeat_count = -1});
	bt.tree.AddNode({.type = ai::BTNodeType::Action, .handler_key = "creature_wander"}, root);
	return bt;
}

// Register the "creature_wander" BT action: amble one tile every moveInterval seconds, staying within
// the wander radius of home and only onto walkable, unblocked tiles.
void RegisterCreatureBtHandlers(const flecs::world& world, flecs::entity tilemapEntity) {
	auto& reg = world.get_mut<ai::BTHandlerRegistry>();
	reg.actions["creature_wander"] = [tilemapEntity](flecs::entity e, ai::Blackboard&, float dt) -> ai::BTStatus {
		auto* wander = e.try_get_mut<CreatureWander>();
		auto* gp = e.try_get_mut<tilemap::GridPosition>();
		if (wander == nullptr || gp == nullptr) {
			return ai::BTStatus::Failure;
		}

		// Amble slowly: wait moveInterval between steps so creatures saunter rather than sprint.
		wander->timer -= dt;
		if (wander->timer > 0.0f) {
			return ai::BTStatus::Running;
		}
		wander->timer = wander->moveInterval;

		// 0..7 pick a direction; 8..11 idle this beat (so a creature sometimes pauses in place).
		const std::uint32_t roll = NextRng(wander->rng) % 12u;
		if (roll >= 8u) {
			return ai::BTStatus::Running;
		}
		const glm::ivec2 step = kDirs[roll];
		const int nx = gp->x + step.x;
		const int nz = gp->z + step.y;

		// Stay within the wander radius of home (Chebyshev distance).
		if (std::abs(nx - wander->home.x) > wander->radius || std::abs(nz - wander->home.y) > wander->radius) {
			return ai::BTStatus::Running;
		}

		// Only step onto a walkable, unblocked tile. Resolve the TileSet via the tilemap's UseRegistry
		// target (as grid movement does); fall back to bounds + blocking-override when it is absent.
		if (const auto* tm = tilemapEntity.try_get<tilemap::Tilemap>()) {
			const flecs::entity regEnt = tilemapEntity.target<level::UseRegistry>();
			const auto* ts = regEnt.is_valid() ? regEnt.try_get<tilemap::TileSet>() : nullptr;
			const bool ok = (ts != nullptr)
								? tilemap::IsTileWalkable(*tm, *ts, nx, nz)
								: (tilemap::InBounds(*tm, nx, nz) && !tilemap::IsTileBlocked(*tm, nx, nz));
			if (!ok) {
				return ai::BTStatus::Running;
			}
		}

		gp->x = nx;
		gp->z = nz;
		return ai::BTStatus::Running;
	};
}

} // namespace

void RegisterCreatureSystems(flecs::world& world, flecs::entity tilemapEntity) {
	// Pre-register creature component types in this non-system context so the physics module's
	// multi-stage (readonly) world never lazily creates them from inside a system (which crashes).
	world.component<CreatureTag>();
	world.component<CreatureKind>();
	world.component<CreatureWander>();

	RegisterCreatureBtHandlers(world, tilemapEntity);
}

void SpawnCreatures(flecs::world& world, flecs::entity sceneRoot, const std::vector<CreatureSpawn>& records) {
	int index = 0;
	for (const CreatureSpawn& rec : records) {
		const auto e = world.entity().child_of(sceneRoot);
		e.add<CreatureTag>();
		e.set<CreatureKind>({.kind = rec.kind});

		// Per-creature wander RNG seeded from the spawn tile + index: varied motion, deterministic setup.
		std::uint32_t seed = static_cast<std::uint32_t>(rec.tile.x) * 73856093u
							 ^ static_cast<std::uint32_t>(rec.tile.y) * 19349663u
							 ^ static_cast<std::uint32_t>(index) * 83492791u;
		if (seed == 0u) {
			seed = 1u;
		}
		e.set<CreatureWander>({
			.home = rec.tile,
			.radius = std::max(1, rec.wanderRadius),
			.moveInterval = 1.2f,
			.timer = static_cast<float>(index % 7) * 0.17f, // stagger first steps so they don't march in lockstep
			.rng = seed,
		});

		e.set<tilemap::GridPosition>({.x = rec.tile.x, .z = rec.tile.y});
		e.set<ai::Blackboard>({});
		e.set<ai::BehaviorTree>(MakeCreatureBtTree());
		++index;
	}
	engine::ecs::Log(world).info("[CreatureSystem] Spawned {} creature(s)", records.size());
}

void AttachCreatureSprites(flecs::world& world, flecs::entity sceneRoot, const CreatureCatalog& catalog) {
	const flecs::entity objectSheet = sceneRoot.lookup("ObjectSheet");
	const flecs::entity objectRegistry = sceneRoot.lookup("FarmObjectRegistry");
	if (!objectSheet.is_valid() || !objectRegistry.is_valid()) {
		return; // headless / no GPU: creatures still exist and wander, just without a quad
	}

	const auto* atlas = objectSheet.try_get<render::SpriteAtlas>();
	const float atlasW = atlas ? static_cast<float>(atlas->image_width_px) : 1.0f;
	const float atlasH = atlas ? static_cast<float>(atlas->image_height_px) : 1.0f;
	const auto* tileset = objectRegistry.try_get<tilemap::TileSet>();

	const auto resolveRect = [&](const std::string& sprite) -> std::optional<core::Rect> {
		if (tileset == nullptr || sprite.empty()) {
			return std::nullopt;
		}
		if (const auto it = tileset->name_to_id.find(sprite); it != tileset->name_to_id.end()) {
			if (const auto* desc = tileset->GetTile(it->second)) {
				return desc->tex_coords;
			}
		}
		return std::nullopt;
	};

	// Collect first, then mutate — adding components to the iterated entities would move their table.
	std::vector<flecs::entity> creatures;
	world.query_builder<const CreatureTag>().build().each([&creatures](flecs::entity e, const CreatureTag&) {
		creatures.push_back(e);
	});

	for (const flecs::entity e : creatures) {
		const auto* ck = e.try_get<CreatureKind>();
		const CreatureDef* def = ck ? catalog.Find(ck->kind) : nullptr;
		const std::string sprite = def ? def->sprite : std::string{};
		const float height = def ? def->height : 0.6f;

		const auto* gp = e.try_get<tilemap::GridPosition>();
		const glm::vec3 pos{
			(static_cast<float>(gp ? gp->x : 0) + 0.5f) * kStep,
			0.0f,
			(static_cast<float>(gp ? gp->z : 0) + 0.5f) * kStep,
		};
		e.set<spatial::Transform>({.position = pos, .rotation = {0.0f, 0.0f, 0.0f}, .scale = {1.0f, 1.0f, 1.0f}});
		e.set<tilemap::GridMover>({.origin = pos, .target = pos, .moving = false});
		e.add<UprightMover>();
		e.add<render::UseSpriteAtlas>(objectSheet);
		e.set<render::FrustumCullRadius>({});  // cull when off-screen

		// One mesh per creature: the DeleteDynamicMesh OnRemove observer unloads each handle on
		// teardown, so handles must not be shared between entities.
		const int handle =
			UploadUprightQuadMesh(world, kCreatureWidth, height * kStep, resolveRect(sprite), atlasW, atlasH);
		if (handle >= 0) {
			e.set<render::DynamicMesh>({handle});
		}
	}
	engine::ecs::Log(world).info("[CreatureSystem] Attached sprites to {} creature(s)", creatures.size());
}

} // namespace game::farm
