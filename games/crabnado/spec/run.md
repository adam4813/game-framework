# The run

Back to the [spec](../spec.md).

## User stories

1. As a player, I want a title screen with a Play button, so that I can start a run when I'm ready.
2. As a player, I want to move my hermit crab with WASD or the arrow keys, so that I can dodge the horde.
3. As a player, I want my hermit crab to scuttle sideways when it moves, so that it reads as a crab.
4. As a player, I want my weapons to fire on their own, so that I can focus on movement and the waves.
5. As a player, I want to see the crabnado roaming through the middle of the arena, not parked at its edge, so that
   enemies come from changing directions and I can use the rocks against them.
6. As a player, I want enemies to fly out of the crabnado in an arc and land near me, so that spawns are visible and
   I can react before they reach me.
7. As a player, I want enemies to stop being harmless only once they land, so that a crab in the air never hits me.
8. As a player, I want the spawn rate to rise over the run, so that the pressure builds.
9. As a player, I want a "crabnado surges" moment every ~45 seconds that throws a burst of enemies, so that the run
   has peaks.
10. As a player, I want defeated enemies to drop shells, so that I have a reason to go into danger.
11. As a player, I want shells within a short range to fly to me, so that collecting them isn't fiddly.
12. As a player, I want to molt (level up) when I've collected enough shells, so that I get stronger over the run.
13. As a player, I want the game to pause when I molt and show 3 upgrade choices I pick with 1/2/3 (or a click), so
    that I can choose without being hit.
14. As a player, I want each molt to need more shells than the last, so that upgrades stay meaningful.
15. As a player, I want to see my HP, so that I know how close I am to losing.
16. As a player, I want touching an enemy to drain my HP over time, so that being surrounded is dangerous.
17. As a player, I want the run to end when my HP reaches 0, so that mistakes matter.
18. As a player, I want a run clock counting up to 5:00, so that I know when the boss is coming.
19. As a player, I want a results screen showing time survived, kills and molt level, with Retry and Title buttons,
    so that I can see how I did and go again quickly.
20. As a player, I want to pause with Esc and resume or quit to the title, so that I can step away.
21. As a jam judge, I want a full run to take about 5–6 minutes, so that I can finish it in my time per entry.

## Implementation decisions

- **Scenes:** the title scene and the arena scene from the foundation stay. The results screen is a modal over the
  arena scene, not a third scene, so Retry reloads the arena scene in place.
- **Run state** is one singleton holding the run clock, kills, molt level, shells collected and shells needed, and the
  run phase (playing, molting, boss, won, lost). It is reflected so the Remote API can read and set it (e.g. jump the
  clock to 4:50 to check the boss).
- **Molting pauses the run** with the engine's pause-as-data (ADR-0004): simulation systems are `Pausable`, and the
  upgrade UI and input systems are not.
- **Player** is an entity with a player tag, movement speed, HP and max HP, stamina, and pickup range components.
  Input goes through the engine's input layer as game actions (move, dig in, pause), never raw key codes.
- **Crabnado (normal phase)** roams the interior. It travels between waypoints picked inside about 60% of the arena
  radius, and the wind only biases which waypoint it picks next. Playtesting the wave prototype showed that free
  downwind drift pins it against the arena edge, so the horde always arrives from one side and the rocks stop
  mattering tactically. Roaming the interior makes enemies arrive from changing directions around the rocks. It spawns enemies
  by throwing them: each spawned enemy starts at the crabnado's position, aims at a landing point between the
  crabnado and the player (6–32 units from the crabnado, with ±10 units of jitter, clamped to the arena), and flies a
  ballistic arc. From the prototype, the launch speed for a flight time `T` from height `y0` under gravity `G` is:

  ```
  vx = (land.x - start.x) / T;   vz = (land.z - start.z) / T
  vy = (0.5 * G * T * T - y0) / T      // prototype: G = 30, y0 = 16, T in [1.1, 1.6] s
  ```

  An enemy in the air is ignored by collision, damage and the spatial grid until it lands.
- **Spawn rate** starts around 1.6 per second and rises linearly over the run (the prototype used `1.6 + 0.09 * t`).
  The exact curve and the surge size are tuning data, not code.
- **Enemy HP** scales up over the run (the prototype used ×(1 + t / 240)).
- **Shells** carry the enemy type's XP value. Shells needed per molt grow per level (the prototype used
  `6 + level * 5`).
