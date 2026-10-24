#pragma once

#include <flecs.h>

// The player's settings, kept in save slots between sessions: music and sound-effect volume, each with a mute, and
// fullscreen.
namespace crabnado {

// Singleton: the audio settings. Volumes are 0..1 and scale every track's and sound's own level.
struct AudioSettings {
	float music_volume{1.0F};
	float sfx_volume{1.0F};
	bool music_muted{false};
	bool sfx_muted{false};
};

// Singleton: the display settings.
struct DisplaySettings {
	bool fullscreen{false};
};

// A music track's volume before the music setting scales it. Put it beside an audio::Music, and set this instead of
// Music::volume: the ApplyMusicSettings system writes Music::volume from it every frame.
struct MusicLevel {
	float volume{1.0F};
};

// Registers the components and the ApplyMusicSettings system, loads the saved settings (defaults when there are
// none) and applies them to the sound bank and the window. Call after LoadSfx.
void RegisterSettings(const flecs::world& world);

// Replaces the settings, applies them to the sound bank and saves them.
void SetAudioSettings(const flecs::world& world, const AudioSettings& settings);

// Replaces the display settings, applies them to the window and saves them.
void SetDisplaySettings(const flecs::world& world, const DisplaySettings& settings);

} // namespace crabnado
