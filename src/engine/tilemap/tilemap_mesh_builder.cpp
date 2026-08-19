#include "tilemap_mesh_builder.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "engine/ecs/ecs.hpp"
#include "engine/platform/platform.hpp"
#include "engine/render/render.hpp"
#include "tilemap_components.hpp"

namespace engine::tilemap {

void TilemapMeshBuilder::Register(const flecs::world& world) {
	// Observer 1: generates mesh data (vertices, indices, colors) whenever a Tilemap component is set/changed.
	// The TileRegistry is scoped to the tilemap entity, so descriptors are resolved from `e` itself.
	world.observer<Tilemap>("TilemapBuildMesh").event(flecs::OnSet).each([](const flecs::entity e, const Tilemap& tm) {
		// Get the tile registry scoped to this tilemap entity.
		const auto* registry_data = e.try_get<TileRegistry>();
		if (!registry_data) {
			return;
		}

		// Get the TileSet for texture dimensions used to normalise pixel tex_coords → UVs
		const auto* tileset = e.try_get<TileSet>();
		const float tex_w = tileset ? static_cast<float>(tileset->image_width_px) : 1.0f;
		const float tex_h = tileset ? static_cast<float>(tileset->image_height_px) : 1.0f;

		// Guard against malformed data: the flat grid must hold exactly width*height ids, otherwise
		// the inner loop would read past tile_ids. Reject rather than render a corrupt/partial mesh.
		if (tm.tile_ids.size() != static_cast<size_t>(tm.width) * tm.height) {
			spdlog::warn(
				"[Tilemap] tile_ids size {} != width*height ({}x{}); skipping mesh build",
				tm.tile_ids.size(),
				tm.width,
				tm.height
			);
			return;
		}

		TilemapMeshData mesh_data;

		const float tile_size = TileWorldSize(tm); // pixels → world units (per-tilemap scale)

		for (uint32_t y = 0; y < tm.height; ++y) {
			for (uint32_t x = 0; x < tm.width; ++x) {
				const uint32_t tile_idx = y * tm.width + x;
				const uint32_t tile_id = tm.tile_ids[tile_idx];

				// Look up tile descriptor from registry
				const TileDescriptor* descriptor = registry_data->GetTile(tile_id);
				if (!descriptor) {
					continue; // Tile not found in registry, skip
				}

				// Create position for this tile (centre of quad)
				const float world_x = (static_cast<float>(x) + 0.5f) * tile_size;
				const float world_z = (static_cast<float>(y) + 0.5f) * tile_size;

				// Quad vertices (XZ plane at y=0)
				const auto vertex_offset = static_cast<uint32_t>(mesh_data.vertices.size());
				const float half_size = tile_size / 2.0f;

				mesh_data.vertices.emplace_back(world_x - half_size, 0.0f, world_z - half_size);
				mesh_data.vertices.emplace_back(world_x + half_size, 0.0f, world_z - half_size);
				mesh_data.vertices.emplace_back(world_x + half_size, 0.0f, world_z + half_size);
				mesh_data.vertices.emplace_back(world_x - half_size, 0.0f, world_z + half_size);

				// Winding order: CCW
				mesh_data.indices.push_back(vertex_offset + 0);
				mesh_data.indices.push_back(vertex_offset + 2);
				mesh_data.indices.push_back(vertex_offset + 1);

				mesh_data.indices.push_back(vertex_offset + 0);
				mesh_data.indices.push_back(vertex_offset + 3);
				mesh_data.indices.push_back(vertex_offset + 2);

				// Per-vertex color (tint; white means the texture is shown without modification)
				const glm::vec4 color = descriptor->color;
				for (int i = 0; i < 4; ++i) {
					mesh_data.colors.push_back(color);
				}

				// UV coordinates: normalise pixel tex_coords rect to 0-1 range
				const float u0 = descriptor->tex_coords.x / tex_w;
				const float v0 = descriptor->tex_coords.y / tex_h;
				const float u1 = (descriptor->tex_coords.x + descriptor->tex_coords.w) / tex_w;
				const float v1 = (descriptor->tex_coords.y + descriptor->tex_coords.h) / tex_h;

				mesh_data.uvs.emplace_back(u0, v0); // TL
				mesh_data.uvs.emplace_back(u1, v0); // TR
				mesh_data.uvs.emplace_back(u1, v1); // BR
				mesh_data.uvs.emplace_back(u0, v1); // BL
			}
		}

		// Store CPU mesh data temporarily for upload
		e.set<TilemapMeshData>(mesh_data);
	});

	// Observer 2: uploads mesh data to GPU and creates a DynamicMesh component
	world.observer<TilemapMeshData>("UploadTilemapMesh")
		.event(flecs::OnSet)
		.each([](const flecs::entity e, TilemapMeshData& mesh_data) {
			if (e.has<render::DynamicMesh>()) {
				e.remove<render::DynamicMesh>(); // Remove old mesh if it exists
			}

			// Only upload once
			if (mesh_data.vertices.empty() || mesh_data.indices.empty()) {
				return;
			}

			// Upload to GPU via platform backend
			const auto& platform_ref = e.world().get<platform::PlatformRef>();
			const int handle = platform_ref.ptr->UploadDynamicMesh(
				mesh_data.vertices,
				mesh_data.indices,
				mesh_data.colors,
				mesh_data.uvs
			);

			if (handle >= 0) {
				// Set the generic DynamicMesh component
				e.set<render::DynamicMesh>({handle});

				// Ensure entity has Transform for rendering
				if (!e.has<ecs::Transform>()) {
					e.set<ecs::Transform>({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
				}
			}

			// Clean up CPU-side data
			mesh_data.vertices.clear();
			mesh_data.indices.clear();
			mesh_data.colors.clear();
			mesh_data.uvs.clear();
		});
}

} // namespace engine::tilemap
