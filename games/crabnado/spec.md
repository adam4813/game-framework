# Crabnado: spec

Status: ready-for-agent

Game jam entry for the theme **crabs and breezes**. Solo, about 1.5 days, programmer art. Crabnado is its own Game
Target on the shared engine (ADR-0006), shipped as a WASM build for browser play and as a desktop build.

## Problem Statement

Jam players spend a few minutes per entry and judge it on fun and on how well it uses the theme. Our earlier ideas
used crabs as the subject and the breeze only as decoration. The player needs to *feel* the breeze as something to
work with or against, in a game one developer can finish and polish in 1.5 days, on an engine whose horde-scale
performance in the browser is unproven.

## Solution

A survivors-like. You play a **hermit crab** on a shore of tide pools and rocks while a **crabnado** flings crabs,
shrimp and lobsters at you. Weapons fire on their own, and you collect **shells** (XP) to **molt** into upgrades.
The **wind** is weather. It shifts direction with a warning and drives **waves** across the arena. A wave **crest**
**tumbles** the horde, and you either **dig in** (spending stamina) to hold your ground or **surf** the crest for a
burst of speed. Tumbled enemies that hit rocks take heavy damage. The waves start predictable and get chaotic. At
5:00 the crabnado comes ashore as the boss, and you win by luring it in front of rocks so a big crest slams it into
them three times.

The concept was chosen by playing two throwaway three.js prototypes (in the `prototypes/` folder next to this spec);
the sailing concept lost. The Crabnado prototype is the reference for the feel of the horde and weapons. It has no
waves, which are the new mechanic.

## Sections

| Section | Covers |
|---|---|
| [The run](spec/run.md) | Scenes, the 5-minute run, crabnado spawning, shells and molting, death, results |
| [Wind and waves](spec/wind-and-waves.md) | Wind weather, crests, tumble, dig in and stamina, surf, the ramp to chaos |
| [Arena and boss](spec/arena-and-boss.md) | Tide-pool and rock layout, rock slams, calm pools, the crabnado boss |
| [Content](spec/content.md) | Weapons, enemy types, upgrades, and the data that defines them |
| [Presentation](spec/presentation.md) | Camera, water visuals, feedback, HUD, audio |
| [Technical](spec/technical.md) | Game modules, horde performance, spatial grid, determinism, pausing, build targets |
| [Testing](spec/testing.md) | What gets a test and what is checked by playing |

## Build order and cut list

Build in this order, so a playable game exists as early as possible:

1. Performance check: about 1,000 enemies in the WASM build (see [Technical](spec/technical.md)).
2. Run loop: player movement, crabnado spawning, claw snap, shells, HP and death.
3. Wind and waves: crests, tumble, dig in and surf. This is the core of the theme, so it comes before more content.
4. Arena: rocks and pools from level JSON, and rock slams.
5. Content: bubbles, the upgrade screen, shrimp and lobster.
6. Boss, results screen, audio, then polish.

If behind, cut in this order: **Riptide** (ship with claw snap and bubbles), then the **raised tile water** (draw
crests as moving foam bars on flat water).

## Out of Scope

- Procedural arenas, more than one arena, meta-progression between runs, saves.
- Player control of the wind direction. The wind is weather only.
- Sailing / Sail & Haul (the rejected concept).
- Gamepad and touch input. Keyboard only, plus mouse for menus.
- Online leaderboards.
- Engine changes beyond what a ticket proves it needs (e.g. a particle burst API). Game code works around engine gaps
  first.

## Further Notes

- Jam scope beats engineering ceremony: see [Testing](spec/testing.md). Tickets don't require tests or a code-review
  pass by default.
- Domain terms used here (crest, tumble, dig in, surf, set, molt, crabnado) are not in a glossary yet.
  `games/crabnado/GLOSSARY.md` can be added with `/domain-modeling` if they start to drift.
