#pragma once

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

} // namespace engine::audio
