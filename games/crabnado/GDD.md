# Crabnado: game design document

Crabnado is a survivors-like made for a 1.5-day game jam on the theme **crabs and breezes**. You play a hermit crab
on a shore ringed by a magic barrier while a crabnado (a tornado of crabs) flings crabs, shrimp and lobsters at you.
Your weapons fire on their own. You steer, read the wind, and use the waves it drives against the horde. A run is a
campaign of three Acts, about 12 minutes in all, each ending in a boss fight won mostly with waves, not weapons.

This document describes the game as built. The jam's spec, its tickets, and the three.js prototype that set the
feel of the horde and weapons are kept at the `crabnado-jam` tag.

## Design pillars

- **The breeze is a force, not decoration.** The wind drives the waves, and the waves are the player's strongest
  tool. Every big decision is about where you'll be when the next crest arrives.
- **Readable chaos.** Waves start as one steady swell and get chaotic over the campaign. Every threat is
  telegraphed: a wind shift, a big crest, a rogue wave, an infested crest, a boss attack.
- **The campaign fits one sitting.** A judge should finish all three Acts in about 12 minutes. There is no
  meta-progression; each run is self-contained, and its build (molts and upgrades) carries from Act to Act.

## Glossary

| Term | Meaning |
|---|---|
| **Campaign** | The whole run: three Acts, about 12 minutes, defined in `campaign.json`. |
| **Act** | One chapter of the campaign: its own arena tint and rock count, sea escalation, enemy HP scaling, Stages and boss. It ends in a boss and a comic page. Its clock restarts each Act. |
| **Stage** | A stretch of an Act with its own spawn table, spawn rate and gate. The **Boss Stage** is the last, one past the Act's normal Stages. |
| **Gate** | What ends a Stage: a time, a kill quota, both, or a script. |
| **Spawn source** | The spawner that brings an enemy in: the orbiting crabnado, the off-screen spawner, or an infested crest. |
| **Crabnado** | The tornado that throws enemies. Outside a Boss Stage it circles just outside the barrier, untouchable; in a Boss Stage it comes ashore as the boss (Acts 1 and 2) or keeps throwing from its orbit (Act 3). |
| **Orbit** | The crabnado's circling just outside the barrier, counterclockwise at first. A small arrow at the screen edge points at it while it is off-screen. |
| **Breach** | The boss crabnado coming in through the barrier at the player's bearing, harmless and unhurtable for 1.5 s. |
| **Retreat** | The beaten crabnado blown back out over the barrier to its orbit. |
| **Twins** | The two halves of the crabnado after Captain Crab's **split** at 50% HP. They orbit opposite ways and each throw half the horde. |
| **Infested crest** | A crest that carries seahorses in from upwind (Act 3). Its foam turns green and the HUD calls it out. |
| **Beached** | A seahorse or Captain Crab stranded by the water leaving them: harmless to touch and open to weapons until a crest lifts them again. |
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
        ┌─────────────────────────────────────────────────────┘
        ▼
   Act n: 3-2-1-Go! ─▶ Stages ─▶ Boss Stage ──┬─▶ HP 0 ──▶ Results "Swept away" ──▶ Retry (back to Act 1) / Title
        ▲                                     │
        │                                     └─▶ boss beaten ──▶ ACT CLEAR ─▶ comic page ─┬─▶ Acts 1, 2: free molt ─┐
        └────────────────────────────────────────────────────────────────────────────────│─────────────────────────┘
                                                                                         └─▶ Act 3: ending comic ─▶ Results "Hermit Isle holds!"
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

- **Clock:** counts up. The run is driven by `assets/data/crabnado/campaign.json` (see Campaign). Alongside the run
  clock sit the Act clock and the Stage clock, which restart at each Act and Stage.
- **The player:** 100 HP. Touching a landed enemy drains HP at that enemy's contact rate. The crab scuttles sideways
  when it moves. HP 0 ends the run.
- **Spawning:** the crabnado circles the arena 78 units from the centre at about 6 units per second, counterclockwise
  at first. It reverses on each wind shift and speeds up a little in gusts. It starts at least 90° round from the player
  and throws its first enemy 3 s into the Act. It throws each enemy on a ballistic arc (1.5–2.2 s) to a landing point
  12–45 units in from the barrier on the line toward the player, never past the player, ±10 jitter, and a dark shadow
  on the water grows to show where it will land. **An enemy in the air is harmless and can't be hit until it lands,
  and nothing can damage the orbiting crabnado.**
- **Pressure:** each Stage lists its spawn table (enemy type to weight) and a rate: `base + growth * stage_clock`
  spawns per second, capped by the horde cap. The Boss Stage throws at 35% of the last Stage's closing rate. Enemy HP
  is the type's HP × the Act's `mult` × (1 + Act time / 240), and the Act clock restarts each Act, so the `mult`
  (1.0, 1.6, 2.4) is what steps the horde up between Acts. The first throw comes 3 s into the Act.
- **Spawners:** a spawn entry names the spawner that brings it in (a Stage's `source` is the default). `crabnado` throws
  from the orbiting crabnado. `offscreen` (Act 3's horde) throws volleys from an unseen point 85 units out: the rate
  fills a debt, and when it reaches the next volley's size (3-6) that many enemies land at once, from one bearing at
  least 60° from the last volley's, each with its own landing jitter and flight time, under the usual landing shadows.
  An Act sets these in an `"offscreen": {volley_min, volley_max, radius, min_turn_deg}` block, which an `offscreen`
  entry needs. `"crabnado": false` on an Act hides the crabnado through its normal Stages (it returns for the storm-up
  and the boss), and a `crabnado` entry in such a Stage is dropped as a bad reference. Normal Stages only: a Boss
  Stage's enemies still come from the boss fight. `infested_crest` (Act 3's Seahorse and Boss Stages) has no weight: an entry
  `{"source": "infested_crest", "chance", "riders": [min, max], "min_gap"}` infests a crest of the primary swell with
  that chance as it comes within 2.2 s of the upwind barrier, at most one at a time and `min_gap` seconds apart. The
  foam under it turns sickly green, the HUD says "INFESTED CREST!", and its riders (seahorses) spawn on it 15 units out
  and surf in across the barrier, one-way.
- **Shells and molting:** the first molt needs 35 shells, and each molt after needs ×1.4 more. The count carries
  across Acts, and a `both` Stage's shells bonus pays through the same path. A molt pauses the run
  and offers 3 distinct upgrades. After the pick, "Ready…" and "Go!" hold the pause for about a second so you can get
  your hands back on the movement keys.

## Campaign

The run is three Acts, about 12 minutes, defined in `assets/data/crabnado/campaign.json` (reloaded on every Play). Each
Act sets its own arena (rock counts and a water tint), `hp_scale`, `sea` escalation, Stage tables and boss. The numbers
below are the shipped tuning; every one is data.

| | Act 1: The Red Tide | Act 2: The Crabnado's Fury | Act 3: Captain Crab's Cavalry |
|---|---|---|---|
| Target length (boss included) | ~3:30 | ~4:00 | ~4:30 |
| Stages | Crab: `both`, defeat 75 in 1:00, bonus shells. Shrimp: `time`, 1:30. | Shrimp: `time`, 1:30. Lobster: `both`, defeat 240 in 1:40, bonus heal. | Crab: `time`, 1:00. Seahorse: `quota`, defeat 130. Lobster: `both`, defeat 130 in 1:00, bonus reroll. |
| Spawn source | Crabnado | Crabnado | Off-screen spawner (Seahorse Stage and boss add infested crests) |
| Boss | The crabnado (3 slams), ~45-75 s | The enraged crabnado (120 HP), ~60 s | Captain Crab (240 HP), ~75-120 s |
| Enemy HP `mult` | 1.0 | 1.6 | 2.4 |
| Rocks (inner / outer) | 9 / 5 | 11 / 6 | 13 / 7 |
| Sea escalation | Steady swell only | Crossing swells from 0:45 | Crossing swells from 0:20, rogue waves from 1:00 |
| Molts (target) | about 4 | about 3 | about 3 |

Notes on the tuning:

- **A `both` Stage ends at whichever comes first**, so its time is a ceiling: meeting the quota early ends it early
  and pays the bonus. The quotas are set so a competent player meets them near the time limit. Act lengths in the table
  are for a player who doesn't.
- **Gates by Act:** each Act has at least one `both` Stage so the challenge mechanic gets used. The bonuses are
  shells (Act 1), a heal (Act 2) and a reroll (Act 3). The campaign's single `quota` Stage is Act 3's Seahorse Stage,
  because kill pace varies there.
- **Rates:** every Stage starts above where the last one closed, then grows (0.035 per second in Act 1, 0.03 after).
  Act 3's horde comes in volleys from the off-screen spawner, so its Stages feel spikier at the same average rate.
- **Difficulty is carried with the build:** HP scales with the Act's `mult` because the player keeps every molt. Boss HP
  is set so a build made by taking the first offer at every molt clears the daze or beached windows in 3-5 windows.
- **Molt curve:** the count above includes each Act's free molt. The shells curve is `kShellsFirstMolt` and
  `kShellsGrowth` in `crabnado_constants.hpp`; Acts have no shell rules of their own.

## Story

A short golden-age comic frames the campaign. Warlord Snapjaw challenges the Hermit King, the King vows to fight, and
the barrier goes up (the opening comic, before Act 1). Three pages follow each Act (see Flow): the crabnado limps back
and Snapjaw enrages it; it spins off and Snapjaw summons Captain Crab, who claims it; the twins scatter, Captain Crab
dives toward Snapjaw's realm and the barrier fades ("To be continued... beneath the waves?"). The comics never explain
the barrier's rules or show lasting barrier damage. They end on a hook, not a resolution, so the game's last word is
the victory Results.

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

**The ramp to chaos** is spread across the campaign by each Act's `sea` block (Act time; a missing key never
happens):

| Act | Sea state |
|---|---|
| 1 | One steady swell throughout. |
| 2 | One steady swell, then from 0:45 each wind shift leaves the old swell running alongside the new one (fading over 45 s), so crests cross and their heights add. |
| 3 | Crossing swells from 0:20, and rogue waves from 1:00 (from within about 0.6 rad of the wind's bearing, with a 2.6 s warning). |

The HUD's sea-state line reads "Steady swell", "Crossing swells" or "Rogue waves".

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
spawn tables in `campaign.json`: Act 1 uses crab and shrimp, lobsters arrive with Act 2's Lobster Stage, and seahorses
with Act 3's Seahorse Stage. Every landed enemy has the same three states beyond chasing the player: **ride** (a crest
carries it downwind with no contact damage), **wind-up** (stopped and shivering, yellow) and **charge** (a red, dodgeable
dash at where the player stood, then back to chasing); see "Tumble, slams and pools". The seahorse is the exception: it
is never ridden but **surfs**.

| Type | HP | Speed | Contact DPS | XP | Rides on | Role |
|---|---|---|---|---|---|---|
| Crab | 3 | 4.2 | 4 | 1 | any crest | The steady, sideways-walking base enemy (weight 65 in the Crab Stage). |
| Shrimp | 1.5 | 7.5 | 6 | 1 | any crest | Fast, fragile flankers, from Act 1's Shrimp Stage (weight 27). |
| Lobster | 14 | 2.8 | 18 | 4 | big crests only | Slow tanks that make big waves matter. From Act 2's Lobster Stage. |
| Seahorse | 6 | 1.2 | 7 (×1.5 surfing) | 2 | never ridden: it *surfs* | Act 3 only, from infested crests in the Seahorse and Boss Stages. A surfer glows sea-blue, rides its crest and steers across it toward the player until the crest passes them, it hits the downwind barrier or the crest fades. Rocks block it, and a rock hit while surfing is a slam that kills it. Beached, it flops (can't be slammed) toward a pool within 15 units, else upwind, and the next crest re-grabs it; in a pool it swims at a player sharing the pool, else waits at the upwind edge. Weapons hurt it in every state. |

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
  phase)` (for the crabnado, phase 2 after the first slam, and so on; for Captain Crab, phase 2 is the split), `void OnBossDefeated(const string &in id)`,
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

## The bosses

Every Act ends in a Boss Stage, and none of the bosses can be beaten by shooting alone: the player has to use the
waves (and, in Acts 2 and 3, hit the boss while a slam has left it open). When the last Stage ends, the crabnado stops
throwing for a fixed 5 s storm-up: it darkens, spins and sways harder, the HUD reads "The storm is rising!" and a warning
sounds. The crabnado slides along its orbit to the point nearest the player and holds there. The arena music plays
throughout; the bosses have no themes of their own (cut, see ticket 12).

### Act 1: the crabnado

After the storm-up a banner announces the boss: the crabnado **breaches** the barrier at that point (the pieces
beside it flare, and it can't hurt or be hurt for the 1.5 s it takes to arrive), then chases the player, throws from
its own body at 35% of the rate, and deals 25 damage per second on contact.
**You can't damage it directly.** A big crest that pushes it into a rock counts as one slam (2.5 s cooldown so one
crest can't count twice). The HUD shows the slams left. **Three slams beat it:** the horde is swept away (no shells, no kills), every shell on the water flies to the
player, and the crabnado is blown back out over the barrier to its orbit. That clears the Act.

### Act 2: the enraged crabnado

The boss is the enraged crabnado (`crabnado_enraged`, red-gray storm). It has 120 HP and a bar under the molt bar.
It chases for 2.5–4.5 s, then winds up one attack (never the same twice running) while its funnel strobes pale at about
8 Hz, and recovers 0.6 s after. **Volley:** six crabs on a ring of 5 units around the player and one on the player are
thrown from it and land after 1.4 s on yellow discs; a landing on the player costs 12 HP. **Suction:** a red ring at 30
units warns for 0.9 s, then for 2.5 s the player inside it is pulled toward the crabnado at 7 u/s; digging in cancels the
pull and outrunning it (11 u/s) escapes. **Slams still happen the same way, but each costs 10 HP and dazes it for 4 s**
("DAZED! 3.2" shows beside the bar): it stops, deals no contact damage and its funnel holds pale. Only while dazed do claw
snaps and bubbles hurt it (bubbles aim at it when it is in range); outside the daze weapons ignore it. A slam while dazed
costs HP but doesn't extend the daze. At 0 HP it retreats like the Act 1 boss. Every number is in `bosses.json`.

### Act 3: Captain Crab

The boss is Captain Crab (`captain_crab`, behavior `captain`, purple storm). As the storm-up starts the crabnado
(he has claimed it) reappears and stops throwing, and the next crest is infested: it carries him in on a big seahorse
with 3 seahorses. Most of the time he lands during the storm-up; crests are 6 s apart, so now and then he lands just
after it. The crabnado never breaches: it keeps orbiting, untouchable, throwing the Boss Stage table at 35% of the rate,
and infested crests keep bringing seahorses. The `offscreen` spawner is off. He has **240 HP** on the boss bar ("CAPTAIN
CRAB") and is the only thing that takes boss damage.
- **Surfing** (the seahorse glows sea-blue): a crest carries him at wave speed, working toward its peak and steering
  toward the player at 5 u/s, for 30 contact damage per second (1.5 times 20). He leaves the crest once it has passed
  the player.
- **Lunge:** surfing within 20 units of the player (at most every 5 s), he stops and a yellow lane tracks the player for
  1 s (he turns yellow), then he dashes 22 units along it at 24 u/s for 40 contact damage per second (he turns red).
- **Slams:** a rock hit while surfing or dashing faster than 6 u/s costs **20 HP** and beaches him. A crest that has
  just picked him up can't slam him for 1 s, so a crest rising beside a rock isn't a free slam.
- **Beached is the weapon window** ("BEACHED! 3.2" beside the bar; he lies over, tinted pale): no contact damage, and
  claw snaps and bubbles hurt him. Ordinary swell crests don't pick him up; only a crest taller than 2.4 (a rogue) does,
  and a tide pool calms those, so a pool keeps the window open. After 5 s a small crest (a rogue of amp 1.4) rises
  under him 1.2 s later and carries him at the player. A missed lunge ends the same way, without a window.
- **Split at 50% HP:** the crabnado tears in two (both flash). The twins slide a quarter turn apart over 2 s, to opposite
  bearings, and orbit opposite ways, each throwing half the horde; they hold their throws for those 2 s. C++ fires
  `OnBossPhase("captain_crab", 2)` and `act3.as` shows "THE CRABNADO SPLITS!" with the `split` sting.
- **Phase 2:** every 5 s one twin strobes pale for 1 s, then throws a volley (as Act 2's) or summons a rogue crest
  (amp 2.6) that rises at the barrier below it and rolls at the player. The twin and the attack alternate, so each twin
  does both. A rogue that carries Captain Crab into a rock is a slam like any other. No suction.
- **Win:** at 0 HP the horde is swept and shells vacuumed, the twins shrink away together over 2.5 s, and he swims out to
  sea and sinks. Then ACT CLEAR, the ending comic and the victory Results. A Remote API playthrough holding the player
  beside him through every window took about 95 s of Boss Stage, with the split at about 27 s.

### Between Acts

The scene stays paused and every clock stands still: "ACT CLEAR" (2.5 s), the comic page the Act's
`comic_after` names (Next and Skip; a missing page is logged and skipped), then the arena resets (rocks re-scattered with the next Act's counts, water re-tinted,
horde and shells cleared, HP and stamina full, the player centered, the crabnado back on its orbit) and a free molt pick
(it doesn't change the molt level or shells). A 3-2-1-Go countdown with the Act's title card starts the next Act. Molts,
upgrades, kills and the campaign clock carry over. After the last Act the ending comic leads to the victory Results.
The Acts are "The Red Tide", "The Crabnado's Fury" and "Captain Crab's Cavalry"; Act 3's Crab, Seahorse and Lobster Stages
throw from the off-screen spawner with no crabnado (the Seahorse Stage adds infested crests); its boss is Captain Crab.

## Presentation

- **Camera:** fixed, north-up, angled top-down, following the player, so wind and wave bearings stay readable.
- **Art:** programmer art from tinted engine primitives (flattened sphere bodies, claw spheres, box legs). Hit
  flashes are a short tint change.
- **Water:** a grid of raised, tinted tiles whose height and color come from the wave field each frame, with foam on
  tall crests, over a seabed and a wider ocean ring.
- **HUD:** HP top left. The shell bar top center, with the molt-level chip on its right. The wind compass top right, with
  two lines under it: where you are in the campaign ("Act 2 · Lobster Stage", "Act 3 · Boss Stage") and the sea state.
  Time and kills lower right. While an orbiting crabnado or twin is off-screen, a small arrow at the screen edge points
  at it (the `offscreen` spawner has no source, so it gets none). The loadout lower left: a slot per weapon with its level and a
  cooldown shade, under the dig-in bar, which dims while exhausted.
- **Font:** Lilita One.
- **Audio:** an underwater music loop in the arena (ducked when a run ends), title music, a game-over loop, and
  generated sound effects for claw snaps (only on a hit), bubble pops, crests, rogue warnings, molts, enemy defeats,
  rock crushes, boss slams, Stage changes, a cleared challenge, the storm-up, Captain Crab's split, death and victory.
  The arena music plays through bosses too: boss themes were cut. Repeated sounds vary in pitch. Buttons click with a claw click. Sources and
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
  `DevCheats.god` keeps the player above 0 HP so a fight can be watched, and `BossState` shows the action, HP and timers of the boss
  with an HP bar (set `gap` to 0 to make the enraged crabnado attack now; set `hp` to see Captain Crab split).
  `CaptainState` shows his fight's progress: arrival, the lift, the split, the twins' turns and the escape.
- **Tuning** lives in `src/crabnado_constants.hpp` and the two JSON registries.
- **Assets:** generated by `tools/make_sfx.py` (sound effects), `tools/make_icons.py` (card and HUD icons) and
  `tools/make_story.py` (comic panels).
- **Tests:** `tests/scene_flow_test.cpp` drives the title ⇄ arena flow through the buttons.

## Open work

- **Human balance playtest:** the campaign was tuned with a scripted bot (circling, always the first molt card, god mode
  so it survives to the bosses) and by measuring boss windows, not with a person. Whether a person finds each Act's
  difficulty right, and Act 3's boss at 240 HP, is unconfirmed.
- **WASM pass:** the browser build hasn't been built or played, and the horde cap needs a browser FPS check (fall
  back toward 400 if it can't hold 60 fps). Ticket 13 does this.
- **Crabnado surges:** a burst of throws every ~45 s for peaks in the run. Never built.
- **Origin flash:** a new enemy may render at the origin for one frame (#33).
