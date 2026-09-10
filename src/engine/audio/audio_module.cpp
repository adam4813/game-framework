#include "audio_module.hpp"

#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "audio_components.hpp"
#include "engine/assets/assets_module.hpp"
#include "engine/level/level.hpp"
#include "engine/platform/platform.hpp"
#include "engine/scripting/scripting.hpp"

namespace engine::audio {

AudioModule::AudioModule(const flecs::world& world) {
	// === Reflection
	world.component<SoundEffect>().member<std::string>("path").member<int>("handle").member<bool>("playing");
	world.component<Music>().member<std::string>("path").member<int>("handle").member<bool>("loop");

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<SoundEffect>());
	scripting::RegisterComponentForScripts(world, world.component<Music>());
	scripting::RegisterComponentMethodForScripts<&SoundEffect::Fire>(world, "Fire");

	// === Level loader ===
	level::RegisterComponentLoader(world, "sound_effect", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<SoundEffect>({.path = assets::ResolveAsset(e.world(), j.value("path", std::string{}))});
	});
	level::RegisterComponentLoader(world, "music", [](const flecs::entity e, const nlohmann::json& j) {
		e.set<Music>({
			.path = assets::ResolveAsset(e.world(), j.value("path", std::string{})),
			.loop = j.value("loop", false),
		});
	});

	// === Path-asset resolvers ===
	assets::RegisterPathAsset<SoundEffect>(world, assets::AssetType::Sound);
	assets::RegisterPathAsset<Music>(world, assets::AssetType::Sound);

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

} // namespace engine::audio
