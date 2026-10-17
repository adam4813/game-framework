# Signal Room: Vertical Slice

Label: wayfinder:map
Source: `D:\src\prototypes\singal-room.md` (GDD draft; read it for setting, station catalog and fiction)

## Destination

A set of implementable specs (one per chunk, written to `.scratch/signal-room/`) for a **vertical slice** of Signal
Room as the `games/signal-room/` Game Target: content for **Shifts 1–2** only, but the full stack around it. That
means single player on keyboard with a helper AI Operator (and Swap), on a co-op-ready Operator model, 3D greybox with a fixed iso camera, requisitions, campaign save, DEFCON, and procedural audio.

## Notes

- **Domain:** game design + engine (C++23 / Flecs 4 / Raylib via `Platform`). Vocabulary lives in
  `games/signal-room/GLOSSARY.md` (game) and `docs/GLOSSARY.md` (engine); see `GLOSSARY-MAP.md`. Use **Shift** for a
  campaign stage, never "level".
- **Skills:** grilling tickets call `grilling` + `domain-modeling`; anything touching ECS design consults
  `flecs-systems`; spec writing follows `AGENTS.md` (data-driven, stations/routes/shifts as JSON).
- **Standing preferences:**
  - Co-op-ready, single player first: slice 1 is one human + AI Operator(s). Every Operator is the same entity
    whether a human or the AI drives it, so human co-op can be added in slice 2.
  - Treat the GDD's numbers as tuning targets. Specs pick starting values and keep them in data.
  - Questions go through the ask-user tool, one round per call (per `AGENTS.md`).
- **Engine facts (2026-10-17):** input is keyboard + mouse only (no gamepad, single `InputState`); audio is
  file-based `SoundEffect`/`Music` only; AI module has BT / GOAP / FSM / Utility / Steering; schema-driven JSON Save
  module exists; Jolt physics exists; no `games/` directory or Game Target scaffold exists yet (ADR-0006 describes it).

## Decisions so far

<!-- one line per closed ticket: [title](issues/NN-slug.md): gist -->

- [Gamepad & per-Operator input through Platform](issues/03-gamepad-multi-input-research.md): raylib 5.5 caps at 4 pads, polled hot-plug, desktop/web differ (digital triggers only); recommend raw pad array in `InputState` + data-driven per-Operator action-mapping layer.
- [Procedural audio on Raylib (desktop + WASM)](issues/04-procedural-audio-research.md): feasible and cheap; one callback-driven synth stream behind `Platform::StartSynth`, hand-rolled DSP in `engine::audio`, atomics for game→audio params; web latency ~43 ms (AudioWorklet deferred), needs click-to-unlock.

## Not yet specified

- **Spec chunking & order.** Which specs the decisions roll up into (e.g. core loop, operators & AI, meta, audio,
  Game Target bootstrap) and the order they're handed off. Becomes clear once the core-loop tickets resolve.
- **Data schema.** JSON shape for stations, routes, tickets, Shifts and upgrades. Depends on the route, ticket and
  condition decisions.
- **Screens & HUD.** Ticket rack, WINDOW clock, briefing card, results/stars screen, requisitions screen, Shift
  picker / campaign flow. What must exist and how it is laid out in greybox.
- **Shift 1 tutorial.** The scripted jam and how much onboarding the slice needs (prompts, gated steps).
- **Procedural audio palette.** Beyond the radio tone: relay chatter, per-machine rattle, steam hiss, stamp thud,
  alarm, ticket buzzer. Scope and synthesis approach graduate after the audio research.
- **Game Target bootstrap.** Standing up `games/signal-room/` per ADR-0006 (CMake, entry point, tests, scenes) and
  whether any engine work (gamepad, audio streams) must land first.
- **Greybox readability & juice.** How stations, carried items and machine states read without art; which of camera
  shake / emergency lighting / particles belong in the slice.
- **Playtest & tuning loop.** How the slice will be tuned (imgui tuning panels, telemetry, Flecs remote checks).

## Out of scope

- Shifts 3–10 and their content: Power Panel, Cipher Rotors, Supply Rack, Antenna, Codebook, Reel-to-Reel, Darkroom,
  Two-Key; blackouts, storms, jamming, saboteur, blast doors, hotline and pneumatic tube. Later efforts.
- Human co-op (split keyboard or gamepads): deferred to slice 2. Slice 1 is single player + AI Operator, but the
  Operator model must not block adding human players later.
- Gamepad support: deferred to slice 2 to keep slice 1 small. Findings stay valid (see the Gamepad research
  ticket), and the per-Operator binding layer should not assume keyboard-only.
- Online co-op (the design shouldn't rule it out, but it isn't specced here).
- Final art, palette and lighting polish beyond greybox.
- Endless mode, environmental-hazard and saboteur expansions (GDD §9).
- Renaming the engine's `*.level.json` / Level concept (planned separately).
