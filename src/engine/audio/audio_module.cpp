#include "audio_module.hpp"

#include <string>

#include <spdlog/spdlog.h>

#include "audio_components.hpp"
#include "engine/assets/assets_module.hpp"
#include "engine/platform/platform.hpp"
#include "engine/scripting/scripting.hpp"

namespace engine::audio {

AudioModule::AudioModule(const flecs::world& world) {
	// === Reflection ===
	// path is embedded directly in SoundEffect/Music. std::string is registered as the opaque
	// "string" Flecs type (engine_context.cpp) so this is valid and scripts can read/write it.
	world.component<SoundEffect>().member<std::string>("path").member<int>("handle").member<bool>("playing");
	world.component<Music>().member<std::string>("path").member<int>("handle").member<bool>("loop");

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<SoundEffect>());
	scripting::RegisterComponentForScripts(world, world.component<Music>());
	scripting::RegisterComponentMethodForScripts<&SoundEffect::Fire>(world, "Fire");

	// === OBSERVER: Resolve SoundEffect handle when the component is set with a non-empty path ===
	// The asset registry deduplicates and reference-counts the load; empty paths resolve to -1.
	world.observer<SoundEffect>("ResolveSoundEffect").event(flecs::OnSet).each([](flecs::entity e, SoundEffect& sfx) {
		sfx.handle = assets::LoadSound(e.world(), sfx.path);
	});

	// === OBSERVER: Release SoundEffect on removal ===
	world.observer<SoundEffect>("ReleaseSoundEffect")
		.event(flecs::OnRemove)
		.each([](const flecs::iter& it, size_t, SoundEffect& sfx) {
			if (!sfx.path.empty()) {
				assets::Release(it.world(), assets::AssetType::Sound, sfx.path);
			}
		});

	// === OBSERVER: Resolve Music handle when the component is set with a non-empty path ===
	world.observer<Music>("ResolveMusic").event(flecs::OnSet).each([](flecs::entity e, Music& music) {
		music.handle = assets::LoadSound(e.world(), music.path);
	});

	// === OBSERVER: Release Music on removal ===
	world.observer<Music>("ReleaseMusic")
		.event(flecs::OnRemove)
		.each([](const flecs::iter& it, size_t, Music& music) {
			if (!music.path.empty()) {
				assets::Release(it.world(), assets::AssetType::Sound, music.path);
			}
		});

	// === SYSTEM: SoundEffectPlayback ===
	world.system<SoundEffect>("SoundEffectPlayback")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, SoundEffect& sfx) {
			if (!sfx.playing) return;
			sfx.playing = false;
			auto* platform = it.world().get<platform::PlatformRef>().ptr;
			if (sfx.handle >= 0) {
				platform->PlaySound(sfx.handle);
			}
		});

	// === Script global: PlaySoundHandle(int handle) ===
	scripting::RegisterGlobalFunctionForScripts(
		world,
		scripting::ScriptMethodSignature{
			.name = "PlaySoundHandle",
			.return_type = scripting::ScriptValueType::MakeVoid(),
			.params = {{.type = scripting::ScriptValueType::MakeInt(), .by_reference = false, .name = "handle"}},
		},
		[](const scripting::ScriptCallContext& ctx, const flecs::world& w) {
			auto* platform = w.get<platform::PlatformRef>().ptr;
			if (const int handle = ctx.GetArgInt(0); handle >= 0) {
				platform->PlaySound(handle);
			}
		}
	);

	spdlog::info("[AudioModule] Registered audio registry, components, resolve observers, and script globals");
}

int RegisterSound(const flecs::world& world, const std::string_view id, const std::string_view path) {
	const int handle = assets::RegisterAlias(world, id, assets::AssetType::Sound, path);
	if (handle < 0) {
		spdlog::warn("[AudioModule] Failed to load sound '{}' from '{}'", id, path);
	}
	return handle;
}

int FindSound(const flecs::world& world, const std::string_view id) { return assets::Find(world, id); }

void PlaySound(const flecs::world& world, const std::string_view id) {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	if (const int handle = FindSound(world, id); handle >= 0) {
		platform->PlaySound(handle);
	}
}

} // namespace engine::audio
