#pragma once

#include <string>
#include <unordered_map>

// Central asset ownership data. The registry gives one place to load, deduplicate,
// reference-count and unload textures/sounds/meshes so individual modules stop juggling raw
// platform handles or maintaining their own parallel caches.
namespace engine::assets {

// Kinds of loadable assets. Selects which platform loader/unloader the registry routes through
// and keeps the three per-type caches from colliding on identical paths.
enum class AssetType : int {
	Texture,
	Sound,
	Mesh,
};

// Tracks a single loaded asset's reference count. The platform backend owns the handle;
// we just track how many logical references exist so we can unload when the count reaches zero.
struct RefCountedAsset {
	int refCount{0};
};

// Central asset ownership singleton, set on the world by AssetModule.
// - `refCounts` tracks how many logical owners requested each "<type>:<path>" asset.
//   When the count reaches zero, the platform resource is unloaded.
// - `aliases` maps a stable string id (e.g. "ui_click") to "<type>:<path>" so game code can
//   request assets by id without knowing the file layout.
// Lifetime: Assets are loaded on first Acquire() and unloaded when the last Release() happens.
// OnRemove observers on asset-bearing components (AlbedoMap, SoundEffect, etc) fire Release()
// to decrement refcounts and prevent leaks.
struct AssetRegistry {
	std::unordered_map<std::string, RefCountedAsset> refCounts; // "<type>:<path>" -> refcount
	std::unordered_map<std::string, std::string> aliases; // id -> "<type>:<path>" key
};

} // namespace engine::assets
