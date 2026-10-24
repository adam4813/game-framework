# Crabnado: spec

Game jam entry for the theme **crabs and breezes**. Solo, about 1.5 days, programmer art. Built as a Game Target
(`games/crabnado/`, see ADR-0006) on the shared engine, shipped as a WASM build for browser play and as a desktop
build.

## Problem

The jam theme has two halves. Earlier concepts used crabs as the subject and the breeze only as decoration. The game
needs the breeze to be a mechanic the player works with or against, inside a scope one person can finish in 1.5 days.

Concept chosen by playing throwaway three.js prototypes (`prototypes/crab-jam-prototype.html`, the Crabnado mode).
A sailing concept, Sail & Haul, was tried and rejected as less fun.

## Solution

A survivors-like. You play a **hermit crab** on a tide-pool shore while a **crabnado** flings crustaceans at you. The
**wind** is weather: it shifts direction and drives **waves** across the arena. Wave crests tumble the horde, and you
either **dig in** to hold your ground or **surf** the crest. A run lasts 5 minutes and ends with a boss: slam the
crabnado into the rocks with a big wave.

## Design

### Run

- 5:00 survival, then the boss. HP reaching 0 ends the run.
- Weapons fire on their own. Enemies drop **shells**, which are XP. Each level-up pauses the game and offers 3
  upgrades picked with 1/2/3.
- The crabnado wanders the arena, drifts downwind and throws enemies in arcs that land around the player. The spawn
  rate ramps up over the run, with a surge every ~45 s.

### Wind

- Wind has a direction and a strength, with gusts.
- The direction shifts every 20-30 s. A warning appears (HUD compass and banner) before the turn completes.
- The wind pushes the player, enemies and projectiles a little. Its main job is to drive the waves.

### Waves

- Crests travel in the wind's direction.
- **Tumble:** a crest knocks enemies over. They are stunned and pushed downwind.
- **Dig in:** hold a key to root in place, so crests don't move you. While dug in you can't move, weapons fire as
  normal, and a **stamina** meter drains. Stamina refills while you aren't dug in.
- **Surf:** if you aren't dug in, a crest carries you, giving a speed burst you can steer.
- **Ramp from predictable to chaotic** (easy to learn, hard to master):
  - 0:00-1:30: one swell with steady sets, every third crest big. A big crest also tumbles lobsters.
  - 1:30-3:00: after each wind shift the old swell keeps running, so crests from two directions cross and stack.
  - 3:00+: rogue waves arrive with a short warning.

### Arena

- One hand-authored tide-pool and rock layout, built from level JSON.
- **Rocks:** a tumbling enemy that hits a rock takes heavy damage. Lining up wave slams is a core skill.
- **Tide pools:** calm water where waves break up, so you don't need to dig in. The horde gathers there too.

### Boss (5:00)

- The crabnado comes ashore and chases the player.
- Big crests are telegraphed. Lure the crabnado in front of rocks so a crest smashes it into them.
- Three slams win the run.

### Content

| Kind     | Items                                                                                  |
|----------|----------------------------------------------------------------------------------------|
| Weapons  | Claw snap (aura around you), Bubbles (fire at the nearest enemy, drift with the wind), Riptide (makes the next crest bigger) |
| Enemies  | Crab (baseline, walks sideways), Shrimp (fast, fragile), Lobster (slow, tanky; only big crests tumble it) |
| Upgrades | About 8: claw damage and radius, bubble count and rate, Riptide strength, move speed, max HP, pickup range, stamina |

### Audio

Free SFX and one music loop, loaded through the assets module.

## Implementation decisions

- **Game Target:** `games/crabnado/` with its own `CMakeLists.txt`, `src/` and `tests/`, linking `engine_lib`
  (ADR-0006). Crabnado-only modules (waves, horde, weapons, upgrades) live under `games/crabnado/src/`.
- **Rendering:** instanced engine primitives. Per-instance tint does not split batches, so hit flashes and the water
  tiles stay in one draw call each (`src/engine/platform/render_batch.hpp`).
- **Wave field:** a single function returning height and push at (x, z, t). Every system that applies wave force
  samples it, and so do the visuals. No physics engine. Jolt is not used.
- **Wave visuals:** a grid of tinted, raised quads driven by the wave field. A `ShaderMap` vertex shader is optional
  polish.
- **Crowding and hit checks:** a game-side uniform spatial grid (one singleton, rebuilt each frame). Don't use the
  engine's steering separation/cohesion for the horde: it compares every agent with every other one
  (`src/engine/ai/steering/steering_module.cpp`).
- **Level-up and HUD:** the retained UI module, with edge-triggered keys for picking upgrades.
- **Timing:** weapon cooldowns and fades use the timer module.

## Risks

- **Entity count in WASM.** Each enemy is a Flecs entity going through transform propagation and the render queue.
  In the first hour, spawn about 1,000 enemies in the WASM build and check FPS. If it falls short, cap the horde at
  about 400 and give lobsters more HP.
- **Hit feedback.** The engine has no one-shot particle burst. Use short-lived emitters, or skip bursts.
- **Waves pushing everyone equally reads as noise.** Tumble, dig-in and surf exist so waves treat the player and the
  horde differently. Tune these early.

## Cut list (in order, if behind)

1. Riptide (ship with claw snap and bubbles).
2. Raised tile-wave visuals. Draw crests as moving foam bars on flat water instead.

## Out of scope

- Procedural arenas, multiple levels, meta-progression and saves.
- Sailing / Sail & Haul (rejected concept).
- Player-controlled wind direction.

## Testing decisions

- Unit-test the pure logic with gtest under `games/crabnado/tests/`: the wave field (height and push at a point and
  time, and the ramp stages), the stamina drain and refill, the spatial grid query, and upgrade rolls.
- Each playable milestone is checked at runtime through the Flecs Remote API before it counts as done (AGENTS.md).
- Build the `desktop` and `wasm` presets and run `ctest --preset desktop-test-release` before calling any ticket
  done.
