#include "sfx.hpp"

#include <string>

#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr std::array<const char*, static_cast<size_t>(Sfx::Count)> kFiles{
	"claw_click.wav", "claw_snap.wav", "bubble_pop.wav", "crest.wav", "molt.wav", "boss_slam.wav", "rogue_warning.wav",
};

} // namespace

void LoadSfx(const flecs::world& world) {
	SfxBank bank;
	for (size_t i = 0; i < kFiles.size(); ++i) {
		// Never released: the cache holds each sound for the app's lifetime.
		bank.handles[i] = assets::LoadSound(world, assets::ResolveAsset(world, std::string{"audio/crabnado/"} + kFiles[i]));
	}
	world.set<SfxBank>(bank);
	ui::SetDefaultClickSound(world, assets::ResolveAsset(world, "audio/crabnado/claw_click.wav"));
}

void PlaySfx(const flecs::world& world, const Sfx sfx) {
	const int handle = world.get<SfxBank>().handles[static_cast<size_t>(sfx)];
	if (handle >= 0) {
		world.get<platform::PlatformRef>().ptr->PlaySound(handle);
	}
}

} // namespace crabnado
