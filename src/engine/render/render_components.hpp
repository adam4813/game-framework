#pragma once

#include <string>

#include <glm/glm.hpp>

#include "engine/platform/platform.hpp"

// Rendering primitives. An entity becomes drawable by combining an ecs::WorldTransform (which
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

// Surface appearance shared by every primitive.
struct Material {
	platform::Rgba color{200, 200, 200, 255};
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
	platform::Rgba color{120, 130, 150, 255};
	float intensity{0.35F};
};

// Directional light (like the sun): parallel rays travelling along `direction`. The first entity
// carrying one drives Phong shading and, when `casts_shadows` is set, planar projected shadows
// onto the plane y = `shadow_ground_y`.
struct DirectionalLight {
	glm::vec3 direction{-0.5F, -1.0F, -0.35F}; // direction the light travels
	platform::Rgba color{255, 244, 214, 255};
	float intensity{1.0F};
	float specular_strength{0.4F};
	float shininess{24.0F};
	bool casts_shadows{true};
	float shadow_ground_y{0.0F};
	platform::Rgba shadow_color{10, 12, 16, 120};
};

// Perspective camera used to drive the 3D pass. view_matrix and projection_matrix are recomputed by
// the render module's CameraTransformUpdate observer.
struct Camera {
	glm::vec3 target{0.0F, 0.0F, 0.0F};
	glm::vec3 up{0.0F, 1.0F, 0.0F};
	float fov{60.0F};
	float aspect_ratio{16.0F / 9.0F};
	float near_plane{0.1F};
	float far_plane{100.0F};
	glm::mat4 view_matrix{1.0F};
	glm::mat4 projection_matrix{1.0F};
};

} // namespace engine::render
