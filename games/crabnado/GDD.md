# Crabnado: game design document

Crabnado is a survivors-like made for a 1.5-day game jam on the theme **crabs and breezes**. You play a hermit crab
on a shore ringed by a magic barrier while a crabnado (a tornado of crabs) flings crabs, shrimp and lobsters at you.
Your weapons fire on their own. You steer, read the wind, and use the waves it drives against the horde. A run lasts
about 5–6 minutes and ends in a boss fight won with waves, not weapons.

This document describes the game as built. The jam's spec and tickets are kept at the `crabnado-jam` tag. The
three.js prototype that set the feel of the horde and weapons is `prototypes/crab-jam-prototype.html`.

## Design pillars

- **The breeze is a force, not decoration.** The wind drives the waves, and the waves are the player's strongest
  tool. Every big decision is about where you'll be when the next crest arrives.
- **Readable chaos.** Waves start as one steady swell and get chaotic over the run. Every threat is telegraphed: a
  wind shift, a big crest, a rogue wave, the boss.
- **Short and complete.** A judge should finish a run in one sitting. There is no meta-progression; each run is
  self-contained.

## Glossary

| Term | Meaning |
|---|---|
| **Crabnado** | The tornado that throws enemies. It roams the arena and becomes the boss at 5:00. |
| **Crest** | A band of raised water moving downwind. It pushes whatever it passes. |
| **Set** | A group of crests about 6 s apart. Every third crest in a set is **big**. |
| **Swell** | One train of crests along one bearing. Later in the run, two swells cross. |
| **Rogue wave** | A single large crest from near the wind's bearing, with a short warning. |
| **Tumble** | A crest stuns an enemy and carries it downwind for about a second. |
| **Rock slam** | A tumbling enemy hits a rock and takes damage scaled by impact speed. |
| **Dig in** | Hold Space to root in place against crests and wind, spending stamina. |
| **Surf** | Ride a crest while not dug in, taking its push as extra speed you can still steer. |
| **Shell** | The XP a defeated enemy drops. Shells within pickup range fly to you. |
| **Molt** | Level up: the run pauses and you pick one of three upgrades. |

## Flow

```text
Title ──Play──▶ intro (crab scurries off the island) ──▶ story comic (Next / Skip)
                                                              │
                                                              ▼
        ┌──────────────── Arena: 3-2-1-Go! countdown ─▶ run ──┬─▶ HP 0 ──▶ Results "Swept away"
        │                                                     └─▶ 3 boss slams ──▶ Results (victory)
        │                                                              │
        └────────────────────── Retry ◀────────────────────────────────┤
Title ◀──────────────────────────────── Title ◀────────────────────────┘
```

- **Title:** a tropical island in open ocean with the crabnado swaying offshore. The logo's C is a snapping claw.
  Play, Credits and Settings buttons. Title music plays.
- **Story comic:** three golden-age comic panels. Warlord Snapjaw issues his challenge, the Hermit King vows to
  fight, and the barrier goes up. One panel at a time with Next, and a Skip button. Space or Enter advances; Esc
  skips.
- **Arena:** a 3-2-1-Go! countdown, then the run. Esc opens the pause menu (Resume, Settings, Quit to title).
- **Results:** a modal over the arena showing time survived, kills and molt level, with Retry (reloads the arena in
  place) and Title. A loss plays a melancholy piano loop; a win plays a victory sting.
- **Settings:** music and sound-effect volume sliders, a Mute box for each, and a Fullscreen box. Saved to the
  `crabnado_settings` save slot and loaded at startup.

## Controls

Keyboard only, plus the mouse for menus.

| Action | Keys |
|---|---|
| Move | WASD or arrow keys |
| Dig in (hold) | Space |
| Pick a molt card | 1 / 2 / 3, or click |
| Confirm a molt card | Enter, or the Confirm button |
| Pause | Esc |

## The run

- **Clock:** counts up. The boss arrives at 5:00.
- **The player:** 100 HP. Touching a landed enemy drains HP at that enemy's contact rate. The crab scuttles sideways
  when it moves. HP 0 ends the run.
- **Spawning:** the crabnado roams between waypoints inside 60% of the arena radius. The wind biases which waypoint
  it picks next, but it never drifts to the edge, so enemies keep arriving from new directions around the rocks. It
  throws each enemy on a ballistic arc toward a landing point between itself and the player (6–32 units out, ±10
  jitter). **An enemy in the air is harmless and can't be hit until it lands.**
- **Pressure:** the spawn rate starts at 1.0 per second and grows by 0.035 per second of run time, capped by the
  horde cap. Enemy HP grows by ×(1 + t / 240). The first throw comes 3 s in.
- **Shells and molting:** the first molt needs 18 shells, and each molt after needs ×1.3 more. A molt pauses the run
  and offers 3 distinct upgrades. After the pick, "Ready…" and "Go!" hold the pause for about a second so you can get
  your hands back on the movement keys.

## Wind and waves

### Wind

The wind is weather the player can't control. It shifts every 20–30 s by 45–90°, turning over a few seconds, and the
HUD warns 3 s ahead ("Wind shifting!"). Gusts briefly raise its strength. The wind pushes the player, the horde,
bubbles and shells slightly; waves, not wind, are the main force. Wind streaks drift across the screen, and the
compass shows bearing and strength.

### Waves

One **wave field** answers "what is the water height and push here, now?" for every system: player, enemies,
bubbles, shells, the boss and the water visuals. Crests travel downwind as bands perpendicular to the wind, in sets
of crests about 6 s apart, every third one big.

**The ramp to chaos:**

| Run time | Sea state |
|---|---|
| 0:00–1:30 | One steady swell. |
| 1:30–3:00 | After each wind shift, the old swell keeps running alongside the new one (fading over 45 s), so crests cross and their heights add. |
| 3:00+ | Rogue waves, from within about 0.6 rad of the wind's bearing, with a 2.6 s warning. |

Big crests are telegraphed about 2 s ahead by a foam line, a HUD label and a sound cue.

### Tumble, slams and pools

- **Tumble:** a crest taller than an enemy type's tumble threshold stuns it and carries it downwind for about 0.9 s,
  then it recovers and comes back. Lobsters only tumble on big crests.
- **Rock slam:** a tumbling enemy that hits a rock fast enough takes damage scaled by impact speed, enough to kill a
  crab or shrimp outright. A non-tumbling enemy is just stopped by the rock. Rocks block the player and the horde.
- **Tide pools** calm the water inside them almost completely. They're safe from crests, but the horde gathers there
  too.

### Dig in and surf

- **Dig in:** while Space is held and stamina remains, the player is rooted (wave and wind push and movement are
  ignored) and weapons keep firing. Stamina drains at 30/s and refills at 18/s. Run it dry and dig in is locked until
  25% has refilled.
- **Surf:** when not dug in, a passing crest's push adds to your steering as a speed burst.

## The arena

A circle of radius 70 marked by a glowing magic barrier (a hovering ward line with 16 rune pillars), with open ocean
to the horizon so swells and rogue waves roll in from outside. Four tide pools are placed by hand. Rocks are scattered
fresh each run from the run's seed: 9 in an inner band (7–40 from the centre) and 5 near the edge (52–63), clear of
pools, the start point and each other. The layout and scatter rules live in `assets/levels/crabnado/arena.level.json`.

## Weapons

| Weapon | Unlock | Behavior |
|---|---|---|
| **Claw snap** | Start | Hits every enemy within 4.5 units, every 0.9 s, for 2 damage. A ring shows its reach. |
| **Bubbles** | Start | Fires at the nearest landed enemy within 30 units every 0.7 s. Bubbles drift with the wind and waves and pop on the first hit. |
| **Riptide** | First-molt upgrade | Every 8 s, makes the next crest 2.2× bigger (as big as a big crest, so it tumbles lobsters). The HUD shows "RIPTIDE CREST in …". |

## Enemies

Defined in `assets/data/crabnado/enemies.json`. `weight` is the relative spawn chance once a type has appeared.

| Type | HP | Speed | Contact DPS | XP | Tumbles on | Appears | Weight | Role |
|---|---|---|---|---|---|---|---|---|
| Crab | 3 | 4.2 | 4 | 1 | any crest | 0:00 | 65 | The steady, sideways-walking base enemy. |
| Shrimp | 1.5 | 7.5 | 6 | 1 | any crest | 0:25 | 27 | Fast, fragile flankers. |
| Lobster | 14 | 2.8 | 18 | 4 | big crests only | 1:00 | 8 | Slow tanks that make big waves matter. |

## Upgrades

Defined in `assets/data/crabnado/upgrades.json`. Each effect names a reflected stat on the player and adds to or
multiplies it, so a new upgrade for an existing stat needs no code. The first molt always offers Riptide in slot 1.
Molt cards show each changed stat as "before → after", green when it helps and red when it hurts.

| Upgrade | Effect |
|---|---|
| Sharper Claws | Claw snap damage +50% |
| Long Reach | Claw snap radius +20% |
| Bubble Burst | +1 bubble per volley |
| Quick Bubbles | Bubbles fire 25% faster |
| Riptide | Unlocks Riptide |
| Stronger Riptide | Riptide cooldown −25%, crests +50% bigger (offered only once Riptide is unlocked) |
| Scuttle | Move speed +12% |
| Thick Shell | Max HP +25 and heal 25 |
| Tide Pull | Shell pickup range +40% |
| Deep Roots | Max stamina +40, refills 30% faster |

## The boss

At 4:30 the crabnado storms up, darker and swaying harder. At 5:00 a banner announces the boss: the same crabnado
comes ashore and chases the player, its spawn rate drops to 35%, and contact with it deals 25 damage per second.
**You can't damage it directly.** A big crest that pushes it into a rock counts as one slam (2.5 s cooldown so one
crest can't count twice). The HUD shows the slams left. **Three slams win**, and the crabnado breaks apart.

## Presentation

- **Camera:** fixed, north-up, angled top-down, following the player, so wind and wave bearings stay readable.
- **Art:** programmer art from tinted engine primitives (flattened sphere bodies, claw spheres, box legs). Hit
  flashes are a short tint change.
- **Water:** a grid of raised, tinted tiles whose height and color come from the wave field each frame, with foam on
  tall crests, over a seabed and a wider ocean ring.
- **HUD:** HP top left. The shell bar top center, with the molt-level chip on its right. The wind compass and sea
  state top right. Time and kills lower right. The loadout lower left: a slot per weapon with its level and a
  cooldown shade, under the dig-in bar, which dims while exhausted.
- **Font:** Lilita One.
- **Audio:** an underwater music loop in the arena (ducked when a run ends), title music, a game-over loop, and
  generated sound effects for claw snaps (only on a hit), bubble pops, crests, rogue warnings, molts, enemy defeats,
  rock crushes, boss slams, death and victory. Repeated sounds vary in pitch. Buttons click with a claw click. Sources and
  licenses: `CREDITS.md`.

## Technical notes

- **Game Target:** Crabnado's modules (run, horde, wind, waves, weapons, upgrades, arena, boss, UI) live under
  `games/crabnado/src/`, never in the engine (ADR-0006).
- **No physics engine.** Movement, knockback, rock collisions and wave push are hand-written distance and velocity
  math.
- **Crowd grid:** one game-side uniform grid (2.2-unit cells), rebuilt each frame from landed enemies and used for
  separation, bubble hits and claw range. Enemies queue behind a touching neighbor rather than stacking.
- **Horde cap:** 1,000 enemies, which held about 60 fps on desktop. It hasn't been checked in the browser.
- **Pausing** is data (ADR-0004): simulation systems are pausable. Molting, the Esc menu and the results screen use
  it.
- **Determinism:** all gameplay randomness (spawns, jitter, wind, rogue waves, rock scatter, upgrade rolls) draws
  from the engine's `RngState`, seeded per run. The seed is `RunState.seed`.
- **Reflection:** every gameplay component and singleton is reflected, so the Flecs Remote API (port 27751) can read
  and set it. For example, set `RunState.clock` to 290 to reach the boss.
- **Tuning** lives in `src/crabnado_constants.hpp` and the two JSON registries.
- **Assets:** generated by `tools/make_sfx.py` (sound effects), `tools/make_icons.py` (card and HUD icons) and
  `tools/make_story.py` (comic panels).
- **Tests:** `tests/scene_flow_test.cpp` drives the title ⇄ arena flow through the buttons.

## Open work

- **Human balance playtest** (`issues/15-balance-pass.md`): a scripted bot died at about 1:40–1:50, so whether a person
  reaches 3:00 and gets the first molt at 20–30 s is unconfirmed.
- **WASM pass:** the browser build hasn't been built or played, and the horde cap needs a browser FPS check (fall
  back toward 400 if it can't hold 60 fps).
- **Crabnado surges:** a burst of throws every ~45 s for peaks in the run. Never built.
- **Origin flash:** a new enemy may render at the origin for one frame (#33).
