# Content

Back to the [spec](../spec.md).

## User stories

1. As a player, I want a claw snap that hits every enemy around me on a short cooldown, so that I always have a way to
   fight back.
2. As a player, I want bubbles that fire at the nearest enemy and drift with the wind, so that aiming relates to the
   breeze.
3. As a player, I want a Riptide weapon that makes the next crest bigger, so that I can set up wave slams.
4. As a player, I want crabs that walk sideways at a steady pace, so that the basic enemy reads as a crab.
5. As a player, I want shrimp that are fast and fragile (from about 0:25), so that I have to deal with flankers.
6. As a player, I want lobsters that are slow and tanky and only tumble on big crests (from about 1:00), so that big
   waves matter.
7. As a player, I want about 8 upgrades that strengthen weapons, movement, HP, pickup range and stamina, so that each
   run grows differently.
8. As a player, I want the first molt to always offer a wave-related upgrade, so that I discover the wave synergy
   early.
9. As a designer, I want enemy types and upgrades defined as data, so that I can tune or add them without a rebuild.

## Implementation decisions

- **Weapons** are a closed set of three, implemented as one system each: claw snap (radius damage on a cooldown),
  bubbles (fire at the nearest landed enemy; bubbles drift with the wind and wave push, and pop on first hit),
  Riptide (on a cooldown, raises the height of the next crest). Each weapon's numbers (damage, radius, cooldown,
  count, speed) are tuning data.
- **Enemy types** are a string-keyed registry loaded from JSON, as the coding standards require for open-ended kinds.
  Each entry has HP, speed, contact radius, contact damage per second, XP value, model scale, colour, walks-sideways,
  the minimum crest size that tumbles it, and the run time it starts appearing. Prototype values to start from:

  | Type    | HP  | Speed | Radius | DPS | XP | Scale | Appears |
  |---------|-----|-------|--------|-----|----|-------|---------|
  | crab    | 3   | 4.2   | 0.9    | 10  | 1  | 1.0   | 0:00    |
  | shrimp  | 1.5 | 7.5   | 0.6    | 6   | 1  | 0.7   | 0:25    |
  | lobster | 14  | 2.8   | 1.4    | 18  | 4  | 1.6   | 1:00    |

- **Upgrades** are a string-keyed registry loaded from JSON. Each entry has a name, description, the stat it changes
  and how (add or multiply), and an optional condition (e.g. only offered once Riptide is unlocked). The roll draws 3
  distinct eligible upgrades from the engine's `RngState`. The first molt always includes a wave-related upgrade.
  Starting list (about 8): sharper claws, longer reach, bubble burst (+1 bubble), quick bubbles, Riptide (unlock, then
  stronger), scuttle (move speed), thick shell (max HP and heal), tide pull (pickup range), deep roots (stamina).
- **Cut list:** Riptide goes first. Its upgrade entries are then removed from the data.
