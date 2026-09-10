#include "tilemap_mesh_builder.hpp"

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "engine/ecs/ecs.hpp"
#include "engine/platform/platform.hpp"
#include "engine/render/render.hpp"
#include "engine/spatial/spatial.hpp"
#include "tilemap_components.hpp"

namespace engine::tilemap {

namespace {

// Compute inset UVs for a tile descriptor, applying a half-pixel border to prevent atlas bleeding.
// Returns {uv_top_left, uv_bottom_right}.
std::pair<glm::vec2, glm::vec2> TileUVs(const TileDescriptor& desc, const float tex_w, const float tex_h) {
	const float px = desc.tex_coords.x;
	const float py = desc.tex_coords.y;
	const float pw = desc.tex_coords.w;
	const float ph = desc.tex_coords.h;

	if (pw <= 0.0f || ph <= 0.0f) {
		// Zero-area rect: use flat color via UV (0,0)
		return {{0.0f, 0.0f}, {0.0f, 0.0f}};
	}

	const glm::vec2 uv0{(px + 0.5f) / tex_w, (py + 0.5f) / tex_h};
	const glm::vec2 uv1{(px + pw - 0.5f) / tex_w, (py + ph - 0.5f) / tex_h};
	return {uv0, uv1};
}

// Append a quad for tile at buffer slot (bx, bz) with the given UVs into mesh_data.
// Each quad is expanded outward by kTileExpand to fill sub-pixel rasterization seams that appear
// in 3D perspective — adjacent tiles don't share vertices so the GPU may leave a hairline gap.
// The expansion is smaller than half a screen pixel so it's visually imperceptible; with NEAREST
// texture filtering the expanded area samples the tile's edge texel (correct colour).
void PushTileQuad(
	TilemapMeshData& mesh_data,
	const int bx,
	const int bz,
	const float tile_size,
	const glm::vec2 uv0,
	const glm::vec2 uv1,
	const glm::vec4& color
) {
	// Expand by ~0.5% of a tile on each side (≈ 0.4 screen-pixels at typical camera height 12).
	const float kExpand = tile_size * 0.005f;
	const float x0 = static_cast<float>(bx) * tile_size - kExpand;
	const float z0 = static_cast<float>(bz) * tile_size - kExpand;
	const float x1 = static_cast<float>(bx + 1) * tile_size + kExpand;
	const float z1 = static_cast<float>(bz + 1) * tile_size + kExpand;

	const auto vertex_offset = static_cast<uint32_t>(mesh_data.vertices.size());

	mesh_data.vertices.emplace_back(x0, 0.0f, z0); // TL
	mesh_data.vertices.emplace_back(x1, 0.0f, z0); // TR
	mesh_data.vertices.emplace_back(x1, 0.0f, z1); // BR
	mesh_data.vertices.emplace_back(x0, 0.0f, z1); // BL

	// CCW winding (two triangles per quad)
	mesh_data.indices.push_back(vertex_offset + 0);
	mesh_data.indices.push_back(vertex_offset + 2);
	mesh_data.indices.push_back(vertex_offset + 1);
	mesh_data.indices.push_back(vertex_offset + 0);
	mesh_data.indices.push_back(vertex_offset + 3);
	mesh_data.indices.push_back(vertex_offset + 2);

	for (int i = 0; i < 4; ++i) mesh_data.colors.push_back(color);

	mesh_data.uvs.emplace_back(uv0.x, uv0.y); // TL
	mesh_data.uvs.emplace_back(uv1.x, uv0.y); // TR
	mesh_data.uvs.emplace_back(uv1.x, uv1.y); // BR
	mesh_data.uvs.emplace_back(uv0.x, uv1.y); // BL
}

} // namespace

void TilemapMeshBuilder::Register(const flecs::world& world) {
	// Observer: generates viewport-sized or full-map mesh data when Tilemap is set/changed.
	world.observer<Tilemap>("TilemapBuildMesh").event(flecs::OnSet).each([](const flecs::entity e, const Tilemap& tm) {
		const auto* registry_data = e.try_get<TileRegistry>();
		if (!registry_data) return;

		const auto* tileset = e.try_get<TileSet>();
		const float tex_w = tileset ? static_cast<float>(tileset->image_width_px) : 1.0f;
		const float tex_h = tileset ? static_cast<float>(tileset->image_height_px) : 1.0f;

		if (tm.tile_ids.size() != static_cast<size_t>(tm.width) * tm.height) {
			spdlog::warn(
				"[Tilemap] tile_ids size {} != width*height ({}x{}); skipping mesh build",
				tm.tile_ids.size(),
				tm.width,
				tm.height
			);
			return;
		}

		const float tile_size = TileWorldSize(tm);
		const auto* viewport = e.try_get<TilemapViewport>();

		// Guard: skip if viewport is present but buffer size hasn't been computed yet
		if (viewport && (viewport->buffer_width <= 0 || viewport->buffer_height <= 0)) {
			return;
		}

		TilemapMeshData mesh_data;

		if (viewport) {
			// Viewport ring-buffer mode: build a fixed bw × bh grid at origin (0,0).
			// The streaming system will later update UVs and entity Transform each frame.
			const int bw = viewport->buffer_width;
			const int bh = viewport->buffer_height;

			mesh_data.vertices.reserve(static_cast<size_t>(bw * bh * 4));
			mesh_data.indices.reserve(static_cast<size_t>(bw * bh * 6));
			mesh_data.colors.reserve(static_cast<size_t>(bw * bh * 4));
			mesh_data.uvs.reserve(static_cast<size_t>(bw * bh * 4));

			for (int bz = 0; bz < bh; ++bz) {
				for (int bx = 0; bx < bw; ++bx) {
					const uint32_t tile_id = GetTileIdAt(tm, bx, bz);
					const auto* desc = registry_data->GetTile(tile_id);
					const auto [uv0, uv1] =
						desc ? TileUVs(*desc, tex_w, tex_h) : std::pair{glm::vec2{0.0f}, glm::vec2{0.0f}};
					const glm::vec4 color = desc ? desc->color : glm::vec4{1.0f};
					PushTileQuad(mesh_data, bx, bz, tile_size, uv0, uv1, color);
				}
			}
		}
		else {
			// Full-map mode: bake every tile into one static mesh.
			mesh_data.vertices.reserve(tm.width * tm.height * 4);
			mesh_data.indices.reserve(tm.width * tm.height * 6);
			mesh_data.colors.reserve(tm.width * tm.height * 4);
			mesh_data.uvs.reserve(tm.width * tm.height * 4);

			for (uint32_t z = 0; z < tm.height; ++z) {
				for (uint32_t x = 0; x < tm.width; ++x) {
					const uint32_t tile_id = tm.tile_ids[z * tm.width + x];
					const auto* desc = registry_data->GetTile(tile_id);
					if (!desc) continue;
					const auto [uv0, uv1] = TileUVs(*desc, tex_w, tex_h);
					PushTileQuad(mesh_data, static_cast<int>(x), static_cast<int>(z), tile_size, uv0, uv1, desc->color);
				}
			}
		}

		e.set<TilemapMeshData>(mesh_data);
	});

	// Observer: uploads CPU mesh data to GPU and sets a DynamicMesh component.
	world.observer<TilemapMeshData>("UploadTilemapMesh")
		.event(flecs::OnSet)
		.each([](const flecs::entity e, TilemapMeshData& mesh_data) {
			if (e.has<render::DynamicMesh>()) {
				e.remove<render::DynamicMesh>();
			}

			if (mesh_data.vertices.empty() || mesh_data.indices.empty()) {
				return;
			}

			const auto& platform_ref = e.world().get<platform::PlatformRef>();
			const int handle = platform_ref.ptr->UploadDynamicMesh(
				mesh_data.vertices,
				mesh_data.indices,
				mesh_data.colors,
				mesh_data.uvs
			);

			if (handle >= 0) {
				e.set<render::DynamicMesh>({handle});

				if (!e.has<spatial::Transform>()) {
					e.set<spatial::Transform>({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
				}
			}

			mesh_data.vertices.clear();
			mesh_data.indices.clear();
			mesh_data.colors.clear();
			mesh_data.uvs.clear();
		});
}

} // namespace engine::tilemap
