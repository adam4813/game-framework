# 02: Core loop: crab vs. crabnado

Spec: [spec](../spec.md). Read: [The run](../spec/run.md), [Technical](../spec/technical.md),
[Content](../spec/content.md) (claw snap only).

**What to build:** The first playable run. In the game scene the player's hermit crab moves with WASD or the arrows,
scuttling sideways. The crabnado wanders the arena and throws crabs in ballistic arcs that land between it and the
player. Landed crabs walk sideways toward the player and crowd without overlapping (game-side spatial grid). Claw
snap fires on its own on a cooldown, damaging every landed enemy in its radius, with a visible ring and a hit flash.
Touching an enemy drains HP, and at 0 HP the run stops (a "Swept away" placeholder is fine until ticket 13). A run
clock counts up, and the spawn rate rises with it. The camera follows the player, north-up.

**Blocked by:** 01 (Horde performance check), which sets the horde cap and the enemy rendering approach.

**Status:** resolved

- [x] Clicking Play starts a run where the player moves, crabs land from the crabnado, chase the player, and die to
      claw snap.
- [x] The crabnado roams the inner part of the arena (waypoints within about 60% of the radius) and never parks at
      the edge, even in steady wind.
- [x] Enemies in the air never deal or take damage until they land.
- [x] Crabs don't stack on top of each other, and crowding cost stays within the horde cap's frame budget.
- [x] HP drains on contact, and the run stops at 0 HP.
- [x] The spawn rate rises over the run and the horde never exceeds the cap.
- [x] Run state (clock, kills, HP) is reflected: queried and set over the Remote API to confirm it (e.g. set HP low and
      watch the run end).
- [x] Spawn randomness draws from `RngState`.
- [x] The scene-flow test still passes, and the Title button still returns to the title.

## Comments

**2026-10-23, desktop, checked over the Remote API (port 27751).**

- Play → a run starts: WASD/arrows move the hermit crab sideways (`crabnado.CrabnadoModule.PlayerIntent` is the
  action layer), the camera follows north-up, and the crabnado throws crabs that chase the player. Claw snap fires
  every 0.9 s with a bead ring and a white hit flash; at 60 s with the player held at full HP, 233 kills.
- Crabnado roam: over 60 s its farthest point from the centre was 38.4 units, inside the 42-unit (60%) waypoint
  limit. There's no wind yet (ticket 04), so "even in steady wind" is checked again there; the wind only biases the
  waypoint pick.
- No airborne enemy was ever hit (sampled every second). Airborne enemies are left out of the crowd grid, so they
  deal and take no damage.
- Crowding: 1,000 crabs packed around a player who couldn't die and with claw damage set to 0 held 59 fps. The
  closest pair was 0.62 apart (median 1.20, touching 1.62), so they don't stack. A soft push alone let the pack
  compress to 0.06, so crabs now queue behind a touching neighbour ahead of them.
- Setting `PlayerStats.hp` to 1 on `Arena.Player` ended the run within a frame or two: `RunState.phase` became
  `Lost`, the clock froze, "Swept away" showed, and Title returned to an unpaused title scene.
- The spawn rate is `1.6 + 0.09 * t` per second, capped at `kHordeCap`. Every spawn roll draws from `RngState`, which
  is reseeded per run; the seed is stored in `RunState.seed`.
- `ctest --preset macos-intel-test-release`: 32/32 pass, scene-flow included.

Not in this ticket: the "crabnado surges" burst every ~45 s (run.md story 9; no ticket covers it yet), and the enemy-type JSON registry (crab stats are constants until ticket 09).
Tuning note: the prototype values are harsh while standing still (11 HP left at 0:14). Revisit once shells and
upgrades land.
