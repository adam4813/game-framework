#pragma once

#include <string>

// Playable audio components. These let an entity carry the audio it should play so higher layers
// (UI, scripting, gameplay systems) trigger audio through data rather than juggling raw platform
// handles. Sound ownership, deduplication and reference-counting live in the assets module
// (engine::assets), which the audio module loads handles through.
namespace engine::audio {

// One-shot sound effect attached to an entity. `path` is loaded to a platform handle on first
// set (via the ResolveSoundEffect observer); call Fire() to request one-shot playback.
// Kept a POD-compatible struct — `path` is a std::string but the component is accessed via ref
// handle in scripts, so bitwise-copy semantics are never needed from the script layer.
// Call Fire() to request playback; the audio module's SoundEffectPlayback system will play the
// sound once and reset `playing` to false, so Fire() is a one-shot trigger, not a continuous flag.
struct SoundEffect {
	std::string path;
	int handle{-1};
	bool playing{false};

	void Fire() { playing = true; }
};

// Streamed music track attached to an entity. `path` is loaded to a platform handle by the
// ResolveMusic observer; the MusicPlayback system starts it once, keeps its stream fed every frame
// and applies `volume`. `playing` is runtime state: it is set once playback has started. Removing the
// component (or destroying the entity) releases the track, which stops it.
struct Music {
	std::string path;
	int handle{-1};
	bool loop{true};
	float volume{1.0F};
	bool playing{false};
};

} // namespace engine::audio
