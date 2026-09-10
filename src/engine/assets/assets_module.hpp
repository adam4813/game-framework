#pragma once

#include <string>
#include <string_view>

#include <flecs.h>

#include "assets_components.hpp"
#include "engine/platform/platform.hpp"

namespace engine::assets {

// Asset module: owns the AssetRegistry singleton and provides a unified, reference-counted
// load/cache API over the platform's texture/sound/mesh loaders. Replaces the per-module ad-hoc
// handle maps so ownership, deduplication and unloading live in one place.
class AssetModule {
public:
	explicit AssetModule(flecs::world& world);
};

// Acquire an asset by path: loads it through the platform on first request (or returns the cached
// handle) and increments its reference count. Returns the platform handle, or -1 on failure. An
// empty path returns -1 without touching the registry.
int Acquire(const flecs::world& world, AssetType type, std::string_view path);

// Look up an already-loaded asset by path without loading or changing its reference count.
// Returns the platform handle, or -1 if it is not loaded.
[[nodiscard]] int Get(const flecs::world& world, AssetType type, std::string_view path);

// Release one reference to an asset previously acquired by path. When the last reference is
// released the underlying platform resource is unloaded. No-op for unknown paths.
void Release(const flecs::world& world, AssetType type, std::string_view path);

// Bind a stable string id to a path and acquire it. Later code resolves the id with Find.
// Returns the platform handle (-1 on failure).
int RegisterAlias(const flecs::world& world, std::string_view id, AssetType type, std::string_view path);

// Resolve a previously registered alias id to its platform handle. Returns -1 if unknown.
[[nodiscard]] int Find(const flecs::world& world, std::string_view id);

// Convenience wrappers around Acquire for each asset kind.
inline int LoadTexture(const flecs::world& world, const std::string_view path) {
	return Acquire(world, AssetType::Texture, path);
}
inline int LoadSound(const flecs::world& world, const std::string_view path) {
	return Acquire(world, AssetType::Sound, path);
}
inline int LoadMesh(const flecs::world& world, const std::string_view path) {
	return Acquire(world, AssetType::Mesh, path);
}

// Register the standard resolve/release observers for a "path asset" component T — any component
// with a `std::string path` and `int handle` whose handle is a registry-loaded asset. On set the
// handle is acquired (deduplicated + ref-counted); on remove one reference is released (unloading
// the resource when the last reference goes). Replaces the per-component resolve/release observer
// pairs that render (AlbedoMap, MeshPrimitive) and audio (SoundEffect, Music) each hand-wrote.
// Observer names follow "Resolve<Component>" / "Release<Component>", matching the previous handlers.
template<typename T>
void RegisterPathAsset(const flecs::world& world, const AssetType type) {
	const std::string name{world.component<T>().name()};
	world.observer<T>(("Resolve" + name).c_str())
		.event(flecs::OnSet)
		.each([type](const flecs::iter& it, size_t, T& asset) {
			asset.handle = Acquire(it.world(), type, asset.path);
		});
	world.observer<T>(("Release" + name).c_str())
		.event(flecs::OnRemove)
		.each([type](const flecs::iter& it, size_t, const T& asset) {
			if (!asset.path.empty()) {
				Release(it.world(), type, asset.path);
			}
		});
}

// Resolve a relative asset path to an absolute path by prefixing the platform asset directory.
// Empty paths stay empty. Centralizes asset path resolution to avoid duplicating the pattern
// across data loaders (level, tilemap) and scene setup code.
[[nodiscard]] inline std::string ResolveAsset(const flecs::world& world, const std::string_view rel) {
	if (rel.empty()) {
		return {};
	}
	return world.get<platform::PlatformRef>().ptr->AssetDirectory() + "/" + std::string{rel};
}

} // namespace engine::assets
