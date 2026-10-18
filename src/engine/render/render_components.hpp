#pragma once

#include <string>

#include <glm/glm.hpp>

#include "engine/core/core.hpp"
#include "engine/platform/platform.hpp"

// Rendering primitives. An entity becomes drawable by combining a spatial::WorldTransform (which
// supplies world-space position/rotation/scale) with exactly one shape component below and a
// Material. The render systems read the WorldTransform matrix and hand it to the platform's 3D
// draw calls, so the shape components only describe the base dimensions of the primitive.
namespace engine::render {

// Axis-aligned box of the given full extents (before the WorldTransform is applied).
struct CubePrimitive {
	glm::vec3 size{1.0F};
};

// Sphere of the given radius.
struct SpherePrimitive {
	float radius{0.5F};
};

// Flat quad on the local XZ plane with the given full width (x) and depth (y).
struct QuadPrimitive {
	glm::vec2 size{1.0F};
};

// Capsule aligned to the local Y axis: two hemispheres of `radius` separated by `height`.
struct CapsulePrimitive {
	float radius{0.5F};
	float height{1.0F};
};

// Model loaded from a file on disk. `handle` is filled lazily by the render system on first
// draw (a value < 0 means "not loaded yet").
struct MeshPrimitive {
	std::string path;
	int handle{-1};
};

// Dynamically uploaded mesh (built from vertices, indices, colors at runtime).
// `handle` is set by the platform when the mesh is uploaded (value < 0 means "not uploaded yet").
struct DynamicMesh {
	int handle{-1};
};

// Per-entity shader override for DynamicMesh rendering. `stages` lists the shader source files
// (resolved to the platform's shader directory); the render observer calls LoadShader on OnSet.
struct ShaderMap {
	std::vector<platform::ShaderStage> stages;
	int handle{-1};
};

// Surface appearance shared by every primitive.
struct Material {
	core::Rgba color{.r = 200, .g = 200, .b = 200, .a = 255};
	bool wireframe{false};
	bool cast_shadow{true};
};

// Convention: each texture-map component carries `std::string path` and `int handle`. They are
// independent Flecs component types (NOT C++ inheritance) so Flecs sees each as a distinct data
// component. The RegisterTextureResolver<T> template in render_module.cpp works on any type that
// has these two fields. Migrating to Flecs relationship pairs (TextureMap, AlbedoTag) later is
// low-effort: pairs would use one component type with a tag discriminator.

// Diffuse / albedo texture. OnSet observer resolves path → handle.
struct AlbedoMap {
	std::string path;
	int handle{-1};
};

// Ambient light singleton: a uniform base illumination applied to every lit surface so shadowed
// faces never go fully black. Set one on the world.
struct AmbientLight {
	core::Rgba color{.r = 120, .g = 130, .b = 150, .a = 255};
	float intensity{0.35F};
};

// Directional light (like the sun): parallel rays travelling along `direction`. The first entity
// carrying one drives Phong shading and, when `casts_shadows` is set, planar projected shadows
// onto the plane y = `shadow_ground_y`.
struct DirectionalLight {
	glm::vec3 direction{-0.5F, -1.0F, -0.35F}; // direction the light travels
	core::Rgba color{.r = 255, .g = 244, .b = 214, .a = 255};
	float intensity{1.0F};
	float specular_strength{0.4F};
	float shininess{24.0F};
	bool casts_shadows{true};
	float shadow_ground_y{0.0F};
	core::Rgba shadow_color{.r = 10, .g = 12, .b = 16, .a = 120};
};

// Relationship: links a renderable entity to a dedicated material entity that carries Material,
// optionally AlbedoMap, and optionally ShaderMap. Registered with flecs::Exclusive so each entity
// can target at most one material at a time. Usage:
//
//   auto mat = world.entity("Material::MyObject")
//       .set<Material>({.color = colors::Accent})
//       .set<AlbedoMap>({.path = "textures/foo.png"});
//   object.add<RenderWith>(mat);
//
// Render systems accept both this relationship pattern and the legacy inline-component pattern
// (Material + optional AlbedoMap + optional ShaderMap directly on the renderable entity), so
// particles and other entities that mutate their material per-frame can continue to use either.
struct RenderWith {};

// Camera projection mode. Perspective uses a vertical field of view; Orthographic uses a fixed
// world-space view height (Camera::ortho_size) for a foreshortened, scale-independent look.
// Default (int) underlying type: Flecs enum reflection reads 4 bytes, so a narrower type would
// serialize the padding after it.
enum class Projection { Perspective, Orthographic };

// Camera projection attributes plus the world up vector. Perspective cameras use `fov`;
// orthographic cameras use `ortho_size` (the vertical view height in world units). `aspect_ratio`,
// `near_plane`, and `far_plane` apply to both modes.
struct Camera {
	glm::vec3 up{0.0F, 1.0F, 0.0F};
	float fov{60.0F};
	float aspect_ratio{16.0F / 9.0F};
	float near_plane{0.1F};
	float far_plane{100.0F};
	Projection projection{Projection::Perspective};
	float ortho_size{20.0F};
};

// Relationship: `camera.add<LookAt>(target)` aims the camera at `target`'s WorldTransform position.
// Exclusive — a camera looks at exactly one entity. Consumed by Render3DBegin and the debug renderer.
struct LookAt {};

// A sprite atlas (texture sheet with uniform or irregular tile layout).
// Carries the atlas image path and its pixel dimensions. Entities that ARE a sprite atlas also
// carry AlbedoMap, Material, and ShaderMap directly — the atlas entity is the material entity.
//
// JSON level loader key: "sprite_atlas" (registered by the module that uses it, e.g. tilemap).
struct SpriteAtlas {
	std::string texture_path;
	uint32_t    image_width_px{128};
	uint32_t    image_height_px{256};
};

// Relationship: `consumer.add<UseSpriteAtlas>(atlas_entity)` pairs a consumer with a sprite-atlas
// entity. The atlas entity carries SpriteAtlas + AlbedoMap + Material + ShaderMap; an observer
// wires consumer.add<RenderWith>(atlas_entity) so the render system uses it as the material source.
// Not exclusive — multiple tilemaps/sprites can share one atlas.
// JSON: "refs": { "use_sprite_atlas": "AtlasEntityName" }
struct UseSpriteAtlas {};

} // namespace engine::render
