#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include <flecs.h>

// Act 3's boss, Captain Crab (the bosses.json `captain` behavior). As the Boss Stage's storm-up starts, the orbiting
// crabnado (claimed by him) stops throwing and the next crest is infested to carry him in on a big seahorse with a few
// seahorses (InfestForCaptain). Then the crabnado orbits, untouchable, throwing the Boss Stage table.
//
// His cycle follows seahorse rules. Surfing, a crest carries him at wave speed while he steers toward the player, with
// surf contact damage; he leaves it once it has passed the player. Near the player he lunges: a yellow lane tracks the
// player through the wind-up, then he dashes along it for double contact damage. A rock hit while surfing or dashing
// above the slam speed costs HP and beaches him. Beached is the weapon window (BossState::action Dazed, so DazedBoss
// lets claw snaps and bubbles hurt him): only a crest taller than `grab_height` picks him up, which a tide pool calms,
// and after `beach_max` seconds a small lift crest rises under him and carries him at the player. Any crest that
// carries him into a rock (a twin's summoned rogue included) is a slam.
//
// At `split_at` of his HP the crabnado splits into twins on opposite bearings, circling opposite ways and each throwing
// half the table, after a throw pause; OnBossPhase(id, 2) fires for scripts. The twins then take turns, `twin_gap`
// apart, with a volley or a summoned rogue rolling from the barrier below them at the player. At 0 HP the horde is
// swept, shells are vacuumed, the twins collapse and he escapes into the sea; then the Act clears (the campaign ends).
namespace crabnado {

// Tag on Captain Crab's root entity.
struct CaptainCrab {};

// Tag on the seahorse he rides: its tint shows what he's doing.
struct CaptainMount {};

// Tag on the crabnado the split adds.
struct TwinCrabnado {};

// Singleton, reflected: the fight's progress beside BossState (his HP, action, position and radius). `entry_called` once
// his crest has been infested; `arrived` once he is on the water; `outside` while he hasn't yet crossed the barrier.
// `surf_dir` is the direction his crest carries him. `lifting` once the lift crest has been summoned under him.
// `aim` is the lunge's direction and `dash_left` its distance to go; `lane` the id of its yellow lane. `immune`,
// `lunge_cooldown` and `slam_cooldown` count down. `split` once the crabnado has split; `twin_timer` counts to the next
// twin attack and `twin_turn` how many have gone. `defeated` once beaten; `escape` the seconds into his escape.
struct CaptainState {
	bool entry_called{false};
	bool arrived{false};
	bool outside{true};
	bool lifting{false};
	glm::vec2 surf_dir{0.0F};
	glm::vec2 aim{0.0F};
	float dash_left{0.0F};
	uint64_t lane{0};
	float immune{0.0F};
	float lunge_cooldown{0.0F};
	float slam_cooldown{0.0F};
	bool split{false};
	float twin_timer{0.0F};
	int twin_turn{0};
	bool defeated{false};
	float escape{0.0F};
};

// Puts Captain Crab on the water at `position` (x, z), surfing along `direction`, as a child of `parent`, and starts his
// HP. Does nothing unless the current boss is a `captain`.
void SpawnCaptain(const flecs::world& world, flecs::entity parent, glm::vec2 position, glm::vec2 direction);

// Starts the fight's state afresh. Call from the arena scene's Load.
void ResetCaptain(const flecs::world& world);

// Registers and reflects the captain components and adds the CaptainFight and CaptainTwins systems. Call from the
// module constructor after RegisterBoss.
void RegisterCaptain(const flecs::world& world);

} // namespace crabnado
