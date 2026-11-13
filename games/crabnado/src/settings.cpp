#include "settings.hpp"

#include <algorithm>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "sfx.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr std::string_view kSaveSlot = "crabnado_settings";
constexpr std::string_view kDisplaySaveSlot = "crabnado_display";

float MusicGain(const AudioSettings& settings) {
	return settings.music_muted ? 0.0F : settings.music_volume;
}

float SfxGain(const AudioSettings& settings) {
	return settings.sfx_muted ? 0.0F : settings.sfx_volume;
}

// Sets every sound in the bank to the sound-effect volume. The UI click shares the ClawClick handle (the asset cache
// loads each path once), so it follows too.
void ApplySfxVolume(const flecs::world& world, const AudioSettings& settings) {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	for (const int handle : world.get<SfxBank>().handles) {
		if (handle >= 0) {
			platform->SetSoundVolume(handle, SfxGain(settings));
		}
	}
}

// The saved settings, or the defaults when the slot is missing or unreadable. Missing fields keep their default.
AudioSettings ReadSettings(const flecs::world& world) {
	AudioSettings settings;
	const std::string text = world.get<platform::PlatformRef>().ptr->ReadSave(kSaveSlot);
	if (text.empty()) {
		return settings;
	}
	const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
	if (!j.is_object()) {
		spdlog::warn("[Settings] Save slot '{}' is not a JSON object; using the defaults", kSaveSlot);
		return settings;
	}
	settings.music_volume = std::clamp(j.value("music_volume", settings.music_volume), 0.0F, 1.0F);
	settings.sfx_volume = std::clamp(j.value("sfx_volume", settings.sfx_volume), 0.0F, 1.0F);
	settings.music_muted = j.value("music_muted", settings.music_muted);
	settings.sfx_muted = j.value("sfx_muted", settings.sfx_muted);
	return settings;
}

void WriteSettings(const flecs::world& world, const AudioSettings& settings) {
	const nlohmann::json j{
		{"music_volume", settings.music_volume},
		{"sfx_volume", settings.sfx_volume},
		{"music_muted", settings.music_muted},
		{"sfx_muted", settings.sfx_muted},
	};
	world.get<platform::PlatformRef>().ptr->WriteSave(kSaveSlot, j.dump());
}

DisplaySettings ReadDisplaySettings(const flecs::world& world) {
	DisplaySettings settings;
	const std::string text = world.get<platform::PlatformRef>().ptr->ReadSave(kDisplaySaveSlot);
	if (text.empty()) {
		return settings;
	}
	const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
	if (!j.is_object()) {
		spdlog::warn("[Settings] Save slot '{}' is not a JSON object; using the defaults", kDisplaySaveSlot);
		return settings;
	}
	settings.fullscreen = j.value("fullscreen", settings.fullscreen);
	return settings;
}

} // namespace

void RegisterSettings(const flecs::world& world) {
	world.component<AudioSettings>()
		.member<float>("music_volume")
		.member<float>("sfx_volume")
		.member<bool>("music_muted")
		.member<bool>("sfx_muted");
	world.component<DisplaySettings>().member<bool>("fullscreen");
	world.component<MusicLevel>().member<float>("volume");

	const AudioSettings settings = ReadSettings(world);
	world.set<AudioSettings>(settings);
	ApplySfxVolume(world, settings);

	const DisplaySettings display = ReadDisplaySettings(world);
	world.set<DisplaySettings>(display);
	world.get<platform::PlatformRef>().ptr->SetFullscreen(display.fullscreen);

	// Unscoped and not Pausable, so music follows the setting in every scene, paused or not.
	world.system<audio::Music, const MusicLevel>("ApplyMusicSettings")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, audio::Music& music, const MusicLevel& level) {
			music.volume = level.volume * MusicGain(it.world().get<AudioSettings>());
		});
}

void SetAudioSettings(const flecs::world& world, const AudioSettings& settings) {
	world.set<AudioSettings>(settings);
	ApplySfxVolume(world, settings);
	WriteSettings(world, settings);
}

void SetDisplaySettings(const flecs::world& world, const DisplaySettings& settings) {
	world.set<DisplaySettings>(settings);
	auto* platform = world.get<platform::PlatformRef>().ptr;
	platform->SetFullscreen(settings.fullscreen);
	platform->WriteSave(kDisplaySaveSlot, nlohmann::json{{"fullscreen", settings.fullscreen}}.dump());
}

} // namespace crabnado
