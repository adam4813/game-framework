# Signal Room spec 1: Room & bootstrap

Status: done
Map: [Signal Room: Vertical Slice](../wayfinder/map.md) (Spec 1 packet on
[Spec chunking & hand-off order](../wayfinder/issues/13-spec-chunking-order.md))
Sources: [Room layout, movement & set-down](../wayfinder/issues/07-room-layout-movement.md),
[Operators & Swap](../wayfinder/issues/02-operator-seats-coop-input.md) (camera only),
[Greybox readability & juice](../wayfinder/issues/15-greybox-readability.md)
Base branch: `feat/signal-room` (branch off it, PR back into it; it never merges into `main`)

## Problem Statement

Signal Room has a resolved design for its vertical slice, but there is no game to put it in. There is no
`games/signal-room/` Game Target, no room to stand in, no Operator to move and nothing to pick up. Every later spec
(core loop, Operators & AI, Faults, meta, audio) needs a running room to build on. Until one exists, nobody can judge
the things the design only guessed: whether the 13×9 Layout fits the fixed camera, whether 4.5 m/s and the dash feel
right, whether targeting picks the machine the player meant, and whether the greybox machines read from above.

## Solution

A playable greybox room. Launching the Signal Room executable (desktop or web) drops the player straight into Shift 1's
Layout, seen from a fixed high-angle camera that frames the whole floor. The machines stand on their tables: the
in-service ones (Radio 1, Decoder, Telex) show their silhouettes and idle lamps, and the rest stand Tarped under khaki
cloth. Dressing-prop tarps, walls, the rug and the storm window are there too. The player drives one Operator on the
keyboard. They walk with inertia, dash, slide along furniture and walls, and see whatever they would act on brighten in
their colour. A placeholder Item waits on a Counter. The player can pick it up, carry it (at its carry speed), and set
it down on any free Counter with a dust puff. Everything about the room, the Shift, the movement feel and the greybox
look is data. A developer can retune it from an ImGui panel and reload the Shift without rebuilding.

## Sections

Read this index, then only the sections your ticket names. Each section holds its user stories (numbered across the
whole spec) next to its implementation decisions.

| Section | Covers |
|---|---|
| [Game Target & bootstrap](spec/game-target.md) | The `games/signal-room/` target, its build, assets, modules and the Room scene's load order. |
| [Shift & Layout data](spec/shift-and-layout.md) | The Shift and Layout file schemas, registries keyed by string, load errors and the room's scale. |
| [Operator movement & collision](spec/operator-movement.md) | Walking, dashing, facing, the tuning and bindings files, and 2D collision. |
| [Targeting, pick-up & set-down](spec/targeting-and-items.md) | The Target rule, the interact press, carrying and Counter contents, and the Item catalogue. |
| [Camera & greybox reads](spec/greybox-and-camera.md) | The fixed camera, the greybox catalogue and palette, and how Stations, Tarps and the Target render. |
| [Debugging & tuning](spec/debugging.md) | The Room panel, debug overlays, Explorer-visible components and the platform rule. |
| [Testing decisions](spec/testing.md) | The one test seam (the headless Room harness), what good tests check, and the runtime check. |

**Data files.** All JSON, loaded at Room load and re-read on Reload. Later specs extend these documents and never redefine them.

## Out of Scope

- Station verbs, Transforms and Jobs; real route Items and frequency stripes; Radios' frequencies and the Antenna
  Coupler; Tickets, the Window, scoring, HUD, Briefing, Hints and Orientation (spec 2).
- Where-to-go chevrons and the Bubble (✗ refusal, AI intent): these need Transforms or AI (specs 2 and 3).
- The Seat model, the Seat ring and chevron, Swap, a second Operator, Operator–Operator separation, the AI Operator, the
  per-Operator binding layer (spec 3). Bindings stay keyboard-only here, but game logic reads actions so spec 3 can
  swap the source.
- Faults, the Oil Can, emergency lighting, Fault particles (spec 4); Requisitions, save, Title and Shift picker
  (spec 5); all audio (spec 6), so the room is silent.
- Station state animation (blinking lamps, spinning reels, crank, paper). The parts exist and are named, but they stay
  idle.
- Jolt physics, position snap on interaction, camera shake (dropped), gamepad, human co-op.
- Shift 2's own file and content. The shared Layout already contains the Shift 2 machines (Tarped in Shift 1), and
  spec 4 adds Shift 2.

## Further Notes

- Prerequisites are met: the Dear ImGui debug UI and the synth stream have both landed on `feat/signal-room`, and
  [Greybox readability & juice](../wayfinder/issues/15-greybox-readability.md) is resolved. Its three.js prototype
  (branch `prototype/greybox-readability`, variant D) is the visual reference for silhouettes, part placement and the
  Target brighten.
- The engine's Level concept may be renamed later (out of scope on the map). Keep Layout loading behind the game's own
  "load Layout" call so a rename touches one place.
- If a useful term comes up while implementing (for example **Worked side** or **Footprint**), add it to
  `games/signal-room/GLOSSARY.md` rather than inventing synonyms.
- Exit criterion (from the packet): walk, dash and collide through the Shift 1 Layout among in-service and Tarped
  machines, and carry and set down an Item with the Target brighten showing.
