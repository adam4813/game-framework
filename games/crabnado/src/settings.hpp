#pragma once

#include <flecs.h>

// The player's audio settings: music and sound-effect volume, each with a mute, kept in a save slot between sessions.
namespace crabnado {

// Singleton: the audio settings. Volumes are 0..1 and scale every track's and sound's own level.
struct AudioSettings {
	float music_volume{1.0F};
	float sfx_volume{1.0F};
	bool music_muted{false};
	bool sfx_muted{false};
};

// A music track's volume before the music setting scales it. Put it beside an audio::Music, and set this instead of
// Music::volume: the ApplyMusicSettings system writes Music::volume from it every frame.
struct MusicLevel {
	float volume{1.0F};
};

// Registers the components and the ApplyMusicSettings system, loads the saved settings (defaults when there are
// none) and applies them to the sound bank. Call after LoadSfx.
void RegisterSettings(const flecs::world& world);

// Replaces the settings, applies them to the sound bank and saves them.
void SetAudioSettings(const flecs::world& world, const AudioSettings& settings);

} // namespace crabnado
