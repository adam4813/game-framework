#include "assets_module.hpp"

#include <string>

#include <spdlog/spdlog.h>

#include "engine/platform/platform.hpp"

namespace engine::assets {

namespace {

// Compose the cache key so textures, sounds and meshes sharing a path never collide.
std::string MakeKey(const AssetType type, const std::string_view path) {
	return std::to_string(static_cast<int>(type)) + ":" + std::string{path};
}

// Route a load to the correct platform loader. Returns the handle, or -1 on failure.
// The platform backend handles deduplication via g_texture_paths, g_sound_paths, g_model_paths.
int PlatformLoad(platform::Platform* platform, const AssetType type, const std::string_view path) {
	switch (type) {
	case AssetType::Texture: return platform->LoadTexture(path);
	case AssetType::Sound: return platform->LoadSound(path);
	case AssetType::Mesh: return platform->LoadMesh(path);
	}
	return -1;
}

// Route an unload to the correct platform unloader.
void PlatformUnload(platform::Platform* platform, const AssetType type, const int handle) {
	switch (type) {
	case AssetType::Texture: platform->UnloadTexture(handle); break;
	case AssetType::Sound: platform->UnloadSound(handle); break;
	case AssetType::Mesh: platform->UnloadMesh(handle); break;
	}
}

} // namespace

AssetModule::AssetModule(flecs::world& world) {
	world.set<AssetRegistry>({});
	spdlog::info("[AssetModule] Registered asset registry");
}

// Acquire increments the reference count for a "<type>:<path>" key. On first acquire,
// loads through the platform (which deduplicates by path). Returns the platform handle.
int Acquire(const flecs::world& world, const AssetType type, const std::string_view path) {
	if (path.empty() || !world.has<AssetRegistry>()) {
		return -1;
	}
	auto& registry = world.get_mut<AssetRegistry>();
	const std::string key = MakeKey(type, path);

	// Increment refcount; if first acquire, load through platform
	if (const auto it = registry.refCounts.find(key); it != registry.refCounts.end()) {
		++it->second.refCount;
	} else {
		auto* platform = world.get<platform::PlatformRef>().ptr;
		const int handle = PlatformLoad(platform, type, path);
		if (handle < 0) {
			spdlog::warn("[AssetModule] Failed to load asset '{}'", path);
			return -1;
		}
		registry.refCounts[key].refCount = 1;
	}

	// Return the handle from the platform (via a fresh load, which returns cached version)
	auto* platform = world.get<platform::PlatformRef>().ptr;
	return PlatformLoad(platform, type, path);
}

// Get the handle for an asset without affecting refcounts. Returns -1 if not loaded.
int Get(const flecs::world& world, const AssetType type, const std::string_view path) {
	if (path.empty() || !world.has<AssetRegistry>()) {
		return -1;
	}
	const auto& registry = world.get<AssetRegistry>();
	const std::string key = MakeKey(type, path);
	if (registry.refCounts.find(key) == registry.refCounts.end()) {
		return -1;
	}
	// The asset is loaded; fetch it from platform (dedup'd by backend)
	auto* platform = world.get<platform::PlatformRef>().ptr;
	return PlatformLoad(platform, type, path);
}

// Release decrements the reference count. When it reaches zero, unloads the platform resource.
void Release(const flecs::world& world, const AssetType type, const std::string_view path) {
	if (path.empty() || !world.has<AssetRegistry>()) {
		return;
	}
	auto& registry = world.get_mut<AssetRegistry>();
	const std::string key = MakeKey(type, path);
	const auto it = registry.refCounts.find(key);
	if (it == registry.refCounts.end()) {
		return; // Never acquired, nothing to release
	}

	if (--it->second.refCount > 0) {
		return; // Still referenced elsewhere
	}

	// Last reference released; unload through platform
	auto* platform = world.get<platform::PlatformRef>().ptr;
	const int handle = PlatformLoad(platform, type, path);
	if (handle >= 0) {
		PlatformUnload(platform, type, handle);
	}
	registry.refCounts.erase(it);
}

int RegisterAlias(
	const flecs::world& world,
	const std::string_view id,
	const AssetType type,
	const std::string_view path
) {
	if (!world.has<AssetRegistry>()) {
		return -1;
	}
	const int handle = Acquire(world, type, path);
	if (handle >= 0) {
		auto& registry = world.get_mut<AssetRegistry>();
		const std::string id_str{id};
		const std::string key = MakeKey(type, path);
		// Release the id's previous target if it had one
		if (const auto existing = registry.aliases.find(id_str); existing != registry.aliases.end()) {
			Release(world, type, existing->second.substr(existing->second.find(':') + 1));
		}
		registry.aliases[id_str] = key;
	}
	return handle;
}

int Find(const flecs::world& world, const std::string_view id) {
	if (!world.has<AssetRegistry>()) {
		return -1;
	}
	const auto& registry = world.get<AssetRegistry>();
	const auto alias = registry.aliases.find(std::string{id});
	if (alias == registry.aliases.end()) {
		return -1;
	}
	// Extract type and path from the key "<type>:<path>"
	const auto& key = alias->second;
	const size_t colon = key.find(':');
	if (colon == std::string::npos) {
		return -1;
	}
	const int type_int = std::stoi(key.substr(0, colon));
	const auto type = static_cast<AssetType>(type_int);
	const auto path = key.substr(colon + 1);
	// Acquire ensures the asset is loaded and increments refcount
	return Acquire(world, type, path);
}

} // namespace engine::assets
