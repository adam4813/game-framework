#pragma once

#include <string_view>

#include <flecs.h>

namespace engine::audio {

// Flecs module: exposes a string-ID sound API layered over the assets module + platform audio
// backend. Sounds are registered by ID (typically from the game layer at load time) and looked up
// / played by ID, so higher layers never juggle raw platform handles. Handle ownership,
// deduplication and reference-counting are delegated to engine::assets.
class AudioModule {
public:
	explicit AudioModule(const flecs::world& world);
};

// Load `path` through the platform and store its handle under `id` in the AudioAssets registry.
// Returns the platform handle (-1 on failure). Re-registering an existing id overwrites it.
int RegisterSound(const flecs::world& world, std::string_view id, std::string_view path);

// Look up a previously registered sound handle by id. Returns -1 if the id is unknown.
[[nodiscard]] int FindSound(const flecs::world& world, std::string_view id);

// Look up a sound by id and play it through the platform. No-op when the id is unknown.
void PlaySound(const flecs::world& world, std::string_view id);

} // namespace engine::audio
