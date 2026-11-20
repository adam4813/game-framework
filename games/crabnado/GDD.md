# Crabnado: game design document

Crabnado is a survivors-like made for a 1.5-day game jam on the theme **crabs and breezes**. You play a hermit crab
on a shore ringed by a magic barrier while a crabnado (a tornado of crabs) flings crabs, shrimp and lobsters at you.
Your weapons fire on their own. You steer, read the wind, and use the waves it drives against the horde. A run lasts
about 5–6 minutes and ends in a boss fight won with waves, not weapons.

This document describes the game as built. The jam's spec, its tickets, and the three.js prototype that set the
feel of the horde and weapons are kept at the `crabnado-jam` tag.

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
| **Crabnado** | The tornado that throws enemies. Outside a Boss Stage it circles just outside the barrier, untouchable; in the Boss Stage it comes ashore as the boss (5:00 in Act 1). |
| **Crest** | A band of raised water moving downwind. It pushes whatever it passes. |
| **Set** | A group of crests about 6 s apart. Every third crest in a set is **big**. |
| **Swell** | One train of crests along one bearing. Later in the run, two swells cross. |
| **Rogue wave** | A single large crest from near the wind's bearing, with a short warning. |
| **Ride** | A crest grabs an enemy and carries it downwind. Taller crests ride longer. Also called the tumble. |
| **Wind-up** | After a ride, the enemy crouches and shivers, tinted yellow, for about a third of a second. |
| **Charge** | After the wind-up, a red-tinted dash at where the player stood, fast at first and slowing to normal. |
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
        │                                                     └─▶ boss beaten ──▶ next Act (3 Acts) ──▶ Results (victory)
        │                                                              │
        └────────────────────── Retry ◀────────────────────────────────┤
Title ◀──────────────────────────────── Title ◀────────────────────────┘
```

- **Title:** a tropical island in open ocean with the crabnado swaying offshore. The logo's C is a snapping claw.
  Play, Credits and Settings buttons. Title music plays.
- **Story comic:** three golden-age comic panels. Warlord Snapjaw issues his challenge, the Hermit King vows to
  fight, and the barrier goes up. One panel at a time with Next, and a Skip button. Space or Enter advances; Esc
  skips.
- **Story pages between Acts:** three more three-panel pages, `after_act1` (the crabnado limps back, Snapjaw
  enrages it, "Again!"), `after_act2` (it spins off, Snapjaw summons Captain Crab, who claims it) and `ending` (twins
  scatter, Captain Crab dives toward Snapjaw's realm, the barrier fades: "To be continued... beneath the waves?").
  Each is a folder under `assets/textures/crabnado/story/` drawn by `tools/make_story.py --page <id>`. The last
  panel's button reads "Fight!", or "Continue" on the ending. Skipping the ending goes straight to Results.
- **Arena:** a 3-2-1-Go! countdown, then the run. Esc opens the pause menu (Resume, Settings, Quit to title).
- **Results:** a modal over the arena showing time survived, kills and molt level, with Retry (reloads the arena in
  place, back at the Act 1 countdown) and Title. A loss is headed "Swept away" and names where it ended ("Act 2: The
  Crabnado's Fury, Lobster Stage"); a win is headed "Hermit Isle holds!" with the time each Act took (the splits are stored as cumulative clock readings). A loss plays a melancholy piano loop; a win plays a victory sting.
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

- **Clock:** counts up. The run is driven by `assets/data/crabnado/campaign.json`: Acts hold Stages (each ends at a
  gate) and a Boss Stage. Act 1 is the Crab Stage (0:00–0:25), the Shrimp Stage (to 5:00) and then the boss.
- **The player:** 100 HP. Touching a landed enemy drains HP at that enemy's contact rate. The crab scuttles sideways
  when it moves. HP 0 ends the run.
- **Spawning:** the crabnado circles the arena 78 units from the centre at about 6 units per second, counterclockwise
  at first. It reverses on each wind shift and speeds up a little in gusts. It starts at least 90° round from the player
  and throws its first enemy 3 s into the Act. It throws each enemy on a ballistic arc (1.5–2.2 s) to a landing point
  12–45 units in from the barrier on the line toward the player, never past the player, ±10 jitter, and a dark shadow
  on the water grows to show where it will land. **An enemy in the air is harmless and can't be hit until it lands,
  and nothing can damage the orbiting crabnado.**
- **Pressure:** each Stage lists its spawn table (enemy type to weight) and a rate: `base + growth * stage_clock`,
  starting at 1.0 per second and growing by 0.035 per second, capped by the horde cap. The Boss Stage throws at 35% of
  the last Stage's closing rate. Enemy HP grows by ×(1 + act time / 240) (the Act's `hp_scale`). The first throw comes
  3 s in.
- **Spawners:** a spawn entry names the spawner that brings it in (a Stage's `source` is the default). `crabnado` throws
  from the orbiting crabnado. `offscreen` (Act 3's horde) throws volleys from an unseen point 85 units out: the rate
  fills a debt, and when it reaches the next volley's size (3-6) that many enemies land at once, from one bearing at
  least 60° from the last volley's, each with its own landing jitter and flight time, under the usual landing shadows.
  An Act sets these in an `"offscreen": {volley_min, volley_max, radius, min_turn_deg}` block, which an `offscreen`
  entry needs. `"crabnado": false` on an Act hides the crabnado through its normal Stages (it returns for the storm-up
  and the boss), and a `crabnado` entry in such a Stage is dropped as a bad reference. Normal Stages only: a Boss
  Stage's enemies still come from the boss fight.
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

| Act time (the Act's `sea` times; a missing one never happens) | Sea state |
|---|---|
| 0:00–1:30 | One steady swell. |
| 1:30–3:00 | After each wind shift, the old swell keeps running alongside the new one (fading over 45 s), so crests cross and their heights add. |
| 3:00+ | Rogue waves, from within about 0.6 rad of the wind's bearing, with a 2.6 s warning. |

Big crests are telegraphed about 2 s ahead by a foam line, a HUD label and a sound cue.

### Tumble, slams and pools

- **Ride, wind-up, charge:** a crest taller than an enemy type's tumble threshold grabs it and carries it downwind
  for 0.5 s plus 0.6 s per unit of height over the threshold (at most 2 s), with no contact damage. A taller crest
  mid-ride extends the ride, but a ride never restarts. Lobsters only ride big crests.
  - When the ride ends, or the enemy touches the barrier, it winds up for 0.35 s (stopped, shivering, yellow), then
    charges the player's position at launch (red, dodgeable) at 2.2× speed fading to 1× over 1.2 s. A charge pushes
    through the crowd and deals 1.5× contact damage. Then it chases as usual.
  - After a ride, crests can't grab it for a while (1.5 s by default; crab and shrimp 1.0 s, lobster 2.5 s), so the
    horde doesn't clump against the barrier.
  - Every value is a per-type lever in `enemies.json` (`ride_base`, `ride_per_height`, `ride_max`, `ride_push`,
    `immune`, `windup`, `charge_mult`, `charge_time`, `charge_dps_mult`, `charges`), falling back to the defaults in
    `crabnado_constants.hpp`.
- **Rock slam:** a riding enemy that hits a rock fast enough takes damage scaled by impact speed, enough to kill a
  crab or shrimp outright. A non-riding enemy is just stopped by the rock. Rocks block the player and the horde.
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
| **Riptide** | First-molt upgrade | Every 8 s, makes the next crest 2.2× bigger (as big as a big crest, so it rides lobsters). The HUD shows "RIPTIDE CREST in …". |

## Enemies

Defined in `assets/data/crabnado/enemies.json`. When a type appears and its relative spawn weight come from the Stage
spawn tables in `campaign.json`; Act 1 uses crab and shrimp, and lobsters arrive with Act 2.

| Type | HP | Speed | Contact DPS | XP | Rides on | Role |
|---|---|---|---|---|---|---|
| Crab | 3 | 4.2 | 4 | 1 | any crest | The steady, sideways-walking base enemy (Act 1 weight 65). |
| Shrimp | 1.5 | 7.5 | 6 | 1 | any crest | Fast, fragile flankers, from 0:25 (weight 27). |
| Lobster | 14 | 2.8 | 18 | 4 | big crests only | Slow tanks that make big waves matter. Not in Act 1's tables yet. |

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

## Stage gates

A Stage ends on a `time` gate (survive X), a `quota` gate (defeat Y, no clock) or a `both` gate (defeat Y in X: it ends
at whichever comes first, and meeting Y in time pays the Stage's bonus: shells, a molt reroll or a heal; missing costs
nothing). Any enemy death counts toward Y. Every Stage change shows a 2 s banner naming its headliner with a sting; a
`both` Stage adds its challenge ("Defeat 40 in 1:00!") and the HUD shows "Challenge 23/40 · 0:41" while it runs, then
"Challenge cleared!". A reroll adds a "Reroll (n)" button to the molt modal. Gates run on the Stage's own clock, which
(like the run clock) stands still during molts and countdowns.

A fourth gate, `script`, ends only when the Stage's script calls `EndStage()` (see Campaign scripts). A `script` Stage
must name a `script`, and if that script dies it ends after 60 s instead, so a crashed script can't hold the run.

## Campaign scripts

An Act or a Stage can name an AngelScript file in `campaign.json` (`"script": "act3"` loads
`assets/scripts/crabnado/act3.as`). A script reacts to discrete events and can end the Stage, bring the boss, spawn
enemies and show a banner or sting, but it can't change data: there are no setters for spawn tables, rates, HP or enemy
stats. Nothing runs per frame. The Act's script loads at Act start and keeps its globals through the Act's Stages; a
Stage's script loads with its Stage and starts fresh each time. Both are rebuilt on every arena Load.

- **Events** (a script defines only the handlers it wants, written exactly like this; parameter names are free):
  `void OnActStart()`, `void OnStageStart()` (the Boss Stage too), `void OnStageEnd(const string &in reason)` (`"time"`,
  `"quota"`, `"script"` or `"boss"`), `void OnBossStart(const string &in id)`, `void OnBossPhase(const string &in id, int
  phase)` (phase 2 after the first slam, and so on), `void OnBossDefeated(const string &in id)`,
  `void OnKill(const string &in enemyType, const string &in source)` and `void OnTimer(const string &in name)`.
  The Act's handler runs first, then the Stage's. A handler whose parameters match no event is logged at load and never
  runs.
- **Calls:** `EndStage()`, `SpawnBoss(id)`, `Spawn(type, count, source)`, `Banner(text)`, `Sting(id)` (a sound file's
  name without `.wav`) and `After(seconds, name)`, which fires `OnTimer(name)` on the script that set it. Read-only:
  `StageClock()`, `Kills()` (the run's total), `Act()` and `Stage()` (0-based, like `RunState`). Calls queue and apply in
  order at the end of the campaign tick. A call that doesn't fit is logged and ignored: `EndStage` in a Boss Stage, a
  molt or a Transition (in a normal Stage it ends it early, and a `both` bonus is paid only if the quota is met);
  `SpawnBoss` unless it's the Act's Boss Stage, that Act's boss id and the boss isn't up yet (it skips the rest of the
  storm-up); `Spawn` in a molt or between Acts, or for an unknown type or spawner.
- **Errors:** a script that doesn't compile is logged and skipped. A handler that throws is logged once and disables
  that script for the run.
- **Debugging:** `RunState.active_scripts` lists the loaded scripts that are still alive and `last_script_call` the last
  call a script made (with `(refused: why)` if it was ignored). Write `CampaignScripts.fire_event` (plus `fire_arg0`
  and `fire_arg1`) over the Remote API to fire any event by name on the next tick; `CampaignScripts.banner` is the text a
  script's `Banner()` is showing.
- **Tests:** `tests/campaign_script_test.cpp` drives the events and calls on a fixture campaign (`tests/data/`), and
  checks that every script the shipped `campaign.json` names exists, compiles and defines only event handlers.

## The boss

When the last Stage ends, the crabnado stops throwing for a fixed 5 s storm-up: it darkens, spins and sways harder, the HUD
reads "The storm is rising!" and a warning sounds. The crabnado slides along its orbit to the point nearest the
player and holds there. Then a banner announces the boss: the crabnado breaches the barrier at that point (the pieces
beside it flare, and it can't hurt or be hurt for the 1.5 s it takes to arrive), then chases the player, throws from
its own body at 35% of the rate, and deals 25 damage per second on contact.
**You can't damage it directly.** A big crest that pushes it into a rock counts as one slam (2.5 s cooldown so one
crest can't count twice). The HUD shows the slams left. **Three slams beat it:** the horde is swept away (no shells, no kills), every shell on the water flies to the
player, and the crabnado is blown back out over the barrier to its orbit. That clears the Act.

**Act 2's boss is the enraged crabnado** (`crabnado_enraged`, red-gray storm). It has 40 HP and a bar under the molt bar.
It chases for 2.5–4.5 s, then winds up one attack (never the same twice running) while its funnel strobes pale at about
8 Hz, and recovers 0.6 s after. **Volley:** six crabs on a ring of 5 units around the player and one on the player are
thrown from it and land after 1.4 s on yellow discs; a landing on the player costs 12 HP. **Suction:** a red ring at 30
units warns for 0.9 s, then for 2.5 s the player inside it is pulled toward the crabnado at 7 u/s; digging in cancels the
pull and outrunning it (11 u/s) escapes. **Slams still happen the same way, but each costs 10 HP and dazes it for 4 s**
("DAZED! 3.2" shows beside the bar): it stops, deals no contact damage and its funnel holds pale. Only while dazed do claw
snaps and bubbles hurt it (bubbles aim at it when it is in range); outside the daze weapons ignore it. A slam while dazed
costs HP but doesn't extend the daze. At 0 HP it retreats like the Act 1 boss. Every number is in `bosses.json`.

**Between Acts** the scene stays paused and every clock stands still: "ACT CLEAR" (2.5 s), the comic page the Act's
`comic_after` names (Next and Skip; a missing page is logged and skipped), then the arena resets (rocks re-scattered with the next Act's counts, water re-tinted,
horde and shells cleared, HP and stamina full, the player centered, the crabnado back on its orbit) and a free molt pick
(it doesn't change the molt level or shells). A 3-2-1-Go countdown with the Act's title card starts the next Act. Molts,
upgrades, kills and the campaign clock carry over. After the last Act the ending comic leads to the victory Results.
The Acts are "The Red Tide", "The Crabnado's Fury" and "Captain Crab's Cavalry"; Act 3's Crab and Lobster Stages
throw from the off-screen spawner with no crabnado; its boss is still the crabnado until ticket 11 (ticket 10 adds a Seahorse Stage).

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
  and set it. For example, set `RunState.act` and `RunState.stage` to jump to a Stage: Act 1's Boss Stage is `stage` 2 (a Stage's index is its
  position in the Act; the Boss Stage is one past the last). Writing `stage_clock` alone only moves the gate.
  `DevCheats.god` keeps the player above 0 HP so a fight can be watched, and `BossState` shows the enraged boss's action,
  HP and timers (set `gap` to 0 to make it attack now).
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
