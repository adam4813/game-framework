#pragma once

#include <array>

#include <flecs.h>

// Crabnado's sound effects: a bank of platform sound handles, loaded once and played by name.
namespace crabnado {

enum class Sfx { ClawClick, ClawSnap, BubblePop, Crest, Molt, BossSlam, RogueWarning, EnemyDefeat, RockCrush, Count };

// Singleton: the platform handle of each Sfx (-1 when it failed to load or there is no audio device).
struct SfxBank {
	std::array<int, static_cast<size_t>(Sfx::Count)> handles{-1, -1, -1, -1, -1, -1, -1, -1, -1};
	// When each sound last played, in seconds of world time, for the crowded ones' minimum gap.
	std::array<double, static_cast<size_t>(Sfx::Count)> last_played{};
};

// Loads every sound into the SfxBank singleton and makes ClawClick the default UI button click. The sounds stay
// loaded for the life of the app.
void LoadSfx(const flecs::world& world);

// Plays `sfx` once. Does nothing if it didn't load, or if a crowded sound (a defeat or crush) played moments ago.
void PlaySfx(const flecs::world& world, Sfx sfx);

} // namespace crabnado
