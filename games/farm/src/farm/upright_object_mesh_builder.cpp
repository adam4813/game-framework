#include "upright_object_mesh_builder.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

namespace game::farm {

namespace {

constexpr glm::vec4 kWhite{1.0F, 1.0F, 1.0F, 1.0F};

// Raylib's dynamic meshes use 16-bit indices; keep the single baked object mesh under that cap.
constexpr size_t kMaxDynamicMeshVertices = 65536;

// World-unit size of one tile for the moving-quad grid sync (tile_size_px 16 / ppu 32).
constexpr float kTileStep = 0.5F;

// UV corners for an upright quad: {uv_top_left, uv_bottom_right}. A resolved sprite rect is inset by
// half a texel (mirroring the tilemap mesh builder) to prevent atlas bleeding; an unresolved sprite
// spans the whole texture so the quad is still drawn (tinted by its material).
std::pair<glm::vec2, glm::vec2>
UprightUVs(const std::optional<engine::core::Rect>& rect, const float atlas_w, const float atlas_h) {
	if (!rect || rect->w <= 0.0F || rect->h <= 0.0F || atlas_w <= 0.0F || atlas_h <= 0.0F) {
		return {{0.0F, 0.0F}, {1.0F, 1.0F}};
	}
	const glm::vec2 uv0{(rect->x + 0.5F) / atlas_w, (rect->y + 0.5F) / atlas_h};
	const glm::vec2 uv1{(rect->x + rect->w - 0.5F) / atlas_w, (rect->y + rect->h - 0.5F) / atlas_h};
	return {uv0, uv1};
}

// Append one upright quad standing on tile (tile_x, tile_z)'s ground centre, with `color` on every
// vertex (upright.fs multiplies the sprite by it, alpha included).
void PushUprightQuad(
	engine::tilemap::TilemapMeshData& mesh,
	const int tile_x,
	const int tile_z,
	const float tile_size,
	const float half_width,
	const float height,
	const glm::vec2 uv0,
	const glm::vec2 uv1,
	const glm::vec4 color
) {
	const float cx = (static_cast<float>(tile_x) + 0.5F) * tile_size;
	const float cz = (static_cast<float>(tile_z) + 0.5F) * tile_size;

	const auto base = static_cast<uint32_t>(mesh.vertices.size());

	mesh.vertices.emplace_back(cx - half_width, 0.0F, cz);   // BL
	mesh.vertices.emplace_back(cx + half_width, 0.0F, cz);   // BR
	mesh.vertices.emplace_back(cx + half_width, height, cz); // TR
	mesh.vertices.emplace_back(cx - half_width, height, cz); // TL

	// The camera sits at Pz-10 looking toward +Z, so its view-right = (-1,0,0).
	// Triangles BL→BR→TR produce a clockwise silhouette in screen space from that camera
	// (positive-Z normal but camera sees the back), which gets back-face culled.
	// Reversed order BL→TR→BR / BL→TL→TR gives a CCW silhouette (front-facing) from this camera.
	mesh.indices.push_back(base + 0); // BL
	mesh.indices.push_back(base + 2); // TR
	mesh.indices.push_back(base + 1); // BR
	mesh.indices.push_back(base + 0); // BL
	mesh.indices.push_back(base + 3); // TL
	mesh.indices.push_back(base + 2); // TR

	for (int i = 0; i < 4; ++i) mesh.colors.push_back(color);

	// Sprite top (uv0.y) maps to the quad top; sprite bottom (uv1.y) to the ground.
	mesh.uvs.emplace_back(uv0.x, uv1.y); // BL
	mesh.uvs.emplace_back(uv1.x, uv1.y); // BR
	mesh.uvs.emplace_back(uv1.x, uv0.y); // TR
	mesh.uvs.emplace_back(uv0.x, uv0.y); // TL
}

} // namespace

engine::tilemap::TilemapMeshData BuildUprightObjectMesh(
	const std::vector<WorldObject>& objects,
	const ObjectKindRegistry& kinds,
	const UprightMeshConfig& config,
	const SpriteRectResolver& resolveRect
) {
	engine::tilemap::TilemapMeshData mesh;

	const float half_width = config.quadWidthTiles * config.tileSize * 0.5F;
	const glm::vec4 color{1.0F, 1.0F, 1.0F, config.alpha};

	for (const auto& obj : objects) {
		if (obj.height <= 0.0F || obj.footprint.empty()) {
			continue;
		}

		// Resolve the atlas sprite via the kind's data-driven definition, falling back to the raw
		// kind key so an object renders even before its kind is registered.
		const ObjectKindDef* def = FindObjectKindDef(kinds, obj.kind);
		const std::string sprite = (def != nullptr && !def->sprite.empty()) ? def->sprite : obj.kind;
		const std::optional<engine::core::Rect> rect = resolveRect ? resolveRect(sprite) : std::nullopt;
		const auto [uv0, uv1] = UprightUVs(rect, config.atlasWidthPx, config.atlasHeightPx);

		for (const auto& tile : obj.footprint) {
			PushUprightQuad(
				mesh, tile.x, tile.y, config.tileSize, half_width, obj.height * config.tileSize, uv0, uv1, color
			);
		}
	}

	return mesh;
}

namespace {

// Build a SpriteRectResolver backed by a TileSet's named frames: object kinds point at a sprite
// name authored as a tile in the terrain tileset (games/farm/assets/data/tilesets/basic.json). Unknown names
// resolve to nullopt (full-texture quad) without the warning ResolveTileId would log.
SpriteRectResolver MakeTileSetResolver(const engine::tilemap::TileSet* tileset) {
	return [tileset](std::string_view sprite) -> std::optional<engine::core::Rect> {
		if (tileset == nullptr) {
			return std::nullopt;
		}
		const auto it = tileset->name_to_id.find(std::string{sprite});
		if (it == tileset->name_to_id.end()) {
			return std::nullopt;
		}
		const engine::tilemap::TileDescriptor* desc = tileset->GetTile(it->second);
		return desc ? std::optional<engine::core::Rect>{desc->tex_coords} : std::nullopt;
	};
}

} // namespace

void UprightObjectMeshBuilder::Register(const flecs::world& world) {
	// Pre-register the component types in this non-system context so the physics module's multi-stage
	// (readonly) world never has to lazily create them from inside a system (which would crash).
	world.component<WorldObjectLayer>();
	world.component<UprightMover>();

	// OnSet observer: (re)bake the world-object collection into one upright-quad DynamicMesh. Because
	// it fires on every set<WorldObjectLayer>(), re-setting the layer after a fresh generation drops
	// the old GPU mesh and uploads the new one — no stale geometry survives.
	world.observer<WorldObjectLayer>("UprightObjectBuildMesh")
		.event(flecs::OnSet)
		.each([](const flecs::entity e, WorldObjectLayer& layer) {
			// Resolve the TileSet (self, or via the level::UseRegistry target) for sprite frames.
			const auto* tileset = e.try_get<engine::tilemap::TileSet>();
			if (tileset == nullptr) {
				if (const flecs::entity reg = e.target<engine::level::UseRegistry>(); reg.is_valid()) {
					tileset = reg.try_get<engine::tilemap::TileSet>();
				}
			}

			// Resolve the SpriteAtlas (self, or via the render::UseSpriteAtlas target) for UV scale.
			const auto* atlas = e.try_get<engine::render::SpriteAtlas>();
			if (atlas == nullptr) {
				if (const flecs::entity sheet = e.target<engine::render::UseSpriteAtlas>(); sheet.is_valid()) {
					atlas = sheet.try_get<engine::render::SpriteAtlas>();
				}
			}

			const auto* kindsPtr = e.world().try_get<ObjectKindRegistry>();
			const ObjectKindRegistry emptyKinds;
			const ObjectKindRegistry& kinds = kindsPtr ? *kindsPtr : emptyKinds;

			const UprightMeshConfig cfg{
				.tileSize = layer.tileSize,
				.quadWidthTiles = layer.quadWidthTiles,
				.atlasWidthPx = atlas ? static_cast<float>(atlas->image_width_px) : 1.0F,
				.atlasHeightPx = atlas ? static_cast<float>(atlas->image_height_px) : 1.0F,
			};

			// Each object needs 4 vertices for its quad; batch objects so each mesh chunk stays under
			// the 16-bit-index (65 536) GPU cap. This lets play-scale forests render correctly without
			// changing the public API or requiring manual chunk-size authoring.
			constexpr size_t kVertsPerQuad = 4; // one footprint tile = one quad = 4 vertices

			const auto* platform = e.world().try_get<engine::platform::PlatformRef>();
			SpriteRectResolver resolver = MakeTileSetResolver(tileset);

			// Drop any previous GPU mesh and all previous overflow chunks so a rebuild is always clean.
			if (e.has<engine::render::DynamicMesh>()) {
				e.remove<engine::render::DynamicMesh>();
			}
			e.world().defer_begin();
			e.children([](flecs::entity child) {
				if (const char* n = child.name(); n && strstr(n, "_UprightChunk") != nullptr) {
					child.destruct();
				}
			});
			e.world().defer_end();

			if (layer.objects.empty()) {
				return;
			}

			// Find the atlas entity for overflow chunks — they inherit material from the carrier's
			// UseSpriteAtlas rather than via a direct RenderWith add, which may fail in some Flecs
			// execution contexts. Adding UseSpriteAtlas fires the LinkSpriteAtlasToRenderWith observer,
			// which is the reliable path that wires (RenderWith, atlas) on each chunk entity.
			const flecs::entity atlasEntity = e.target<engine::render::UseSpriteAtlas>();

			// Upload one mesh per chunk; the first chunk uses the carrier entity itself, subsequent
			// chunks get named child entities so they are automatically cleaned up when the scene tears
			// down.
			size_t chunkIndex = 0;
			for (size_t start = 0; start < layer.objects.size(); ++chunkIndex) {
				// Grow the chunk object-by-object until the next object's quads would exceed the vertex
				// cap. Counting footprint TILES (not objects) keeps multi-tile objects — building walls,
				// bridge decks — from overflowing a fixed-object-count chunk and being dropped at play
				// scale (which left town buildings invisible and logged an upload error every gen).
				size_t end = start;
				size_t chunkVerts = 0;
				while (end < layer.objects.size()) {
					const size_t objectVerts = layer.objects[end].footprint.size() * kVertsPerQuad;
					if (end > start && chunkVerts + objectVerts > kMaxDynamicMeshVertices) {
						break;
					}
					chunkVerts += objectVerts;
					++end;
				}
				const std::vector<WorldObject> chunk(layer.objects.begin() + static_cast<ptrdiff_t>(start),
				                                     layer.objects.begin() + static_cast<ptrdiff_t>(end));
				start = end; // advance now so the early-continue paths below can never loop forever

				engine::tilemap::TilemapMeshData mesh = BuildUprightObjectMesh(chunk, kinds, cfg, resolver);
				if (mesh.vertices.empty() || mesh.indices.empty()) {
					continue;
				}
				if (platform == nullptr || platform->ptr == nullptr) {
					continue; // headless: geometry built + unit-tested, no GPU upload needed
				}

				const int handle = platform->ptr->UploadDynamicMesh(
					mesh.vertices, mesh.indices, mesh.colors, mesh.uvs
				);
				if (handle < 0) {
					continue;
				}

				if (chunkIndex == 0) {
					// First chunk lives on the carrier entity itself.
					e.set<engine::render::DynamicMesh>({handle});
					if (!e.has<engine::spatial::Transform>()) {
						e.set<engine::spatial::Transform>(
							{.position = {0.0F, 0.0F, 0.0F}, .rotation = {0.0F, 0.0F, 0.0F}, .scale = {1.0F, 1.0F, 1.0F}}
						);
					}
				} else {
					// Overflow chunks become named children so they are torn down with the scene root.
					const std::string chunkName = std::string(e.name()) + "_UprightChunk" + std::to_string(chunkIndex);
					flecs::entity chunk_entity = e.world().entity(chunkName.c_str()).child_of(e);
					chunk_entity.set<engine::render::DynamicMesh>({handle});
					chunk_entity.set<engine::spatial::Transform>(
						{.position = {0.0F, 0.0F, 0.0F}, .rotation = {0.0F, 0.0F, 0.0F}, .scale = {1.0F, 1.0F, 1.0F}}
					);
					// Wire the atlas so LinkSpriteAtlasToRenderWith fires and adds (RenderWith, atlas).
					if (atlasEntity.is_valid()) {
						chunk_entity.add<engine::render::UseSpriteAtlas>(atlasEntity);
					}
				}
			}
		});
}

int UploadUprightQuadMesh(
	const flecs::world& world,
	const float widthWorld,
	const float heightWorld,
	const std::optional<engine::core::Rect> spriteRect,
	const float atlasWidthPx,
	const float atlasHeightPx
) {
	const auto* platform = world.try_get<engine::platform::PlatformRef>();
	if (platform == nullptr || platform->ptr == nullptr || heightWorld <= 0.0F) {
		return -1;
	}

	const float hw = widthWorld * 0.5F;
	const auto [uv0, uv1] = UprightUVs(spriteRect, atlasWidthPx, atlasHeightPx);

	const std::vector<glm::vec3> vertices{
		{-hw, 0.0F, 0.0F},        // BL
		{hw, 0.0F, 0.0F},         // BR
		{hw, heightWorld, 0.0F},  // TR
		{-hw, heightWorld, 0.0F}, // TL
	};
	const std::vector<uint32_t> indices{0, 2, 1, 0, 3, 2}; // BL→TR→BR, BL→TL→TR (CCW from oblique camera)
	const std::vector<glm::vec4> colors(4, kWhite);
	const std::vector<glm::vec2> uvs{
		{uv0.x, uv1.y}, // BL
		{uv1.x, uv1.y}, // BR
		{uv1.x, uv0.y}, // TR
		{uv0.x, uv0.y}, // TL
	};

	return platform->ptr->UploadDynamicMesh(vertices, indices, colors, uvs);
}

void RegisterUprightMoverSystems(const flecs::world& world, const flecs::entity sceneRoot) {
	// Drive each moving entity's GridMover toward whatever tile its GridPosition holds, reusing the
	// engine's GridMoveInterpolate system (registered per scene) to glide the Transform — which is
	// what positions the upright quad each frame via WorldTransform.
	const auto sys =
		world
			.system<engine::tilemap::GridMover, const engine::tilemap::GridPosition, const engine::spatial::Transform>(
				"FarmUprightMoverSync"
			)
			.with<UprightMover>()
			.kind(flecs::OnUpdate)
			.each([](const flecs::iter&,
					 size_t,
					 engine::tilemap::GridMover& mover,
					 const engine::tilemap::GridPosition& gp,
					 const engine::spatial::Transform& tr) {
				if (mover.moving) {
					return; // let the current glide finish before retargeting
				}
				const glm::vec3 desired{
					(static_cast<float>(gp.x) + 0.5F) * kTileStep,
					tr.position.y,
					(static_cast<float>(gp.z) + 0.5F) * kTileStep,
				};
				const float dx = desired.x - tr.position.x;
				const float dz = desired.z - tr.position.z;
				if ((dx * dx + dz * dz) < 1e-6F) {
					return; // already standing on the target tile
				}
				mover.origin = tr.position;
				mover.target = desired;
				mover.duration = std::max(0.05F, std::sqrt(dx * dx + dz * dz) * (0.15F / kTileStep));
				mover.elapsed = 0.0F;
				mover.moving = true;
			})
			.add<engine::ecs::Pausable>();
	sys.child_of(sceneRoot);
}

} // namespace game::farm
