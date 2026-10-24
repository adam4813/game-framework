#include "sfx.hpp"

#include <string>

#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr std::array<const char*, static_cast<size_t>(Sfx::Count)> kFiles{
	"claw_click.wav", "claw_snap.wav", "bubble_pop.wav", "crest.wav", "molt.wav", "boss_slam.wav", "rogue_warning.wav",
	"enemy_defeat.wav", "rock_crush.wav",
};

// Defeats come in crowds, so they get a minimum gap between plays (seconds).
constexpr double MinGap(const Sfx sfx) {
	switch (sfx) {
	case Sfx::EnemyDefeat: return 0.06;
	case Sfx::RockCrush: return 0.1;
	default: return 0.0;
	}
}

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
	auto& bank = world.get_mut<SfxBank>();
	const auto index = static_cast<size_t>(sfx);
	const double now = static_cast<double>(world.get_info()->world_time_total);
	if (bank.handles[index] >= 0 && now - bank.last_played[index] >= MinGap(sfx)) {
		bank.last_played[index] = now;
		world.get<platform::PlatformRef>().ptr->PlaySound(bank.handles[index]);
	}
}

} // namespace crabnado
