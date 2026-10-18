# Signal Room: Vertical Slice

Label: wayfinder:map
Source: `D:\src\prototypes\singal-room.md` (GDD draft; read it for setting, station catalog and fiction)

## Destination

Hand-off packets for six implementable specs (each later written with `/to-spec` outside this map), with every
in-scope decision resolved and the engine prerequisites ticketed, for a **vertical slice** of Signal
Room as the `games/signal-room/` Game Target: content for **Shifts 1–2** only, but the full stack around it. That
means single player on keyboard with a helper AI Operator (and Swap), on a co-op-ready Operator model, 3D greybox with a fixed iso camera, requisitions, campaign save, a Shift picker / campaign flow, and procedural audio.

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
  - Model split: spawn `research` ticket subagents with `model: "sonnet"`. Grilling, prototype and charting stay on
    the main session model (Opus).
- **Engine facts (2026-10-17):** input is keyboard + mouse only (no gamepad, single `InputState`); audio is
  file-based `SoundEffect`/`Music` only; AI module has BT / GOAP / FSM / Utility / Steering; schema-driven JSON Save
  module exists; Jolt physics exists; no `games/` directory or Game Target scaffold exists yet (ADR-0006 describes it).

## Decisions so far

<!-- one line per closed ticket: [title](issues/NN-slug.md): gist -->

- [Gamepad & per-Operator input through Platform](issues/03-gamepad-multi-input-research.md): raylib 5.5 caps at 4 pads, polled hot-plug, desktop/web differ (digital triggers only); recommend raw pad array in `InputState` + data-driven per-Operator action-mapping layer.
- [Routine Cable route & station verbs](issues/01-routine-cable-route.md): item-driven Transforms (N→M, one Job per Station, no queue, pick up outputs). Radio Hold-to-copy (Presets later replaced by fixed-frequency Radios); Decoder walk-away + crank; Telex place + stamp. Items carry a frequency and match the most urgent ticket at the Telex. Counters only. A/B and Shift gimmicks via Modifiers (`keypunch`, `telex_autostamp`).
- [Procedural audio on Raylib (desktop + WASM)](issues/04-procedural-audio-research.md): feasible and cheap; one callback-driven synth stream behind `Platform::StartSynth`, hand-rolled DSP in `engine::audio`, atomics for game→audio params; web latency ~43 ms (AudioWorklet deferred), needs click-to-unlock.
- [Operators & Swap](issues/02-operator-seats-coop-input.md): Seat model (controls + one held Operator; unseated = AI Operator); per-Shift Roster (2 in slice), one-key instant Swap in Roster order; left-behind Operator finishes its Hold/carry, arriving cancels the AI action with ~0.25 s coyote time on Holds; Seat ring + chevron, AI intent icons; fixed whole-room iso camera.
- [Room layout, movement & set-down](issues/07-room-layout-movement.md): Shift names a Layout (grid Level JSON) + in-service Stations, the rest Tarped; one 13×9 Layout for Shifts 1–2 with 6 Counters; 4.5 m/s walk, 2.2×/0.2 s dash, per-Item carry speed; hand-rolled 2D collision with soft Operator separation; facing-cell targeting with cone fallback, no position snap.
- [Tickets, window & scoring for Shifts 1–2](issues/05-tickets-window-scoring.md): NORMAL/HIGH/FLASH (90/65/40 s, base 20/30/40), optional heads-up as gauge overfill (Stand-by → On Air), Radio copies only On Air frequencies; seeded Collection Schedule + pinned entries, rack cap 5; value × band share (100/75/50%) × Handler Chain (×1→×2); Lost Intercept −10; hard stop at 4:00; stars at 40/65/90% of par; newly earned stars are the requisition currency, spent on unlocks.
- [Decoder Faults & the Oil Can in Shift 2](issues/06-condition-heat-overdrive.md): no Condition meter, heat or Overdrive. From Shift 2 the Decoder Faults at random while running (hidden run-time cooldown, seeded): a Jam resets progress (Mash 3×), a Seize pauses (Hold with the reusable Oil Can, from the Counter at (0,8)). Shift 1 has no Faults (scripted jam dropped); Shift 2 pins a Seize at ~20 s.
- [AI Operator behaviour](issues/08-ai-operator-behaviour.md): hand-rolled Job board + scorer (kind weight + urgency + value at stake − distance); copies only to meet On Air demand (pre-positioning later dropped); soft AI-only Claims; reaction delay, ~90% walk and job speed, no dash; Faults preempt after a 1.5/3 s grace (+50% if the human is closer); Seize: park Item, fetch Oil Can, leave it near the Decoder; no commands; swappable AI profile JSON.
- [Requisitions for a two-Shift slice](issues/10-requisitions.md): room-wide, permanent star-bought unlocks, each a catalogue entry wrapping a Modifier (override schema gains `scale`); base catalogue of 4 always on offer (Geared Crank 1★, Long-Throw Antenna 1★, Mil-Spec Tubes 2★, Field Repair Kit 2★ = the 6★ pool); pure upgrades, scarcity is the choice; gating uses stars earned, spending only the Star bank.
- [Radio presets & spectrum read](issues/12-radio-tuning-prototype.md): spectrum + Presets dropped (played like a console minigame); each Radio is a fixed-frequency Station with a display + traffic lamp, 1 in Shift 1, 3 in Shift 2, one-decimal half-MHz labels; engaging a Radio couples the single antenna to it and darkens the rest, then they come back after a `coupler_reset` (< 0.5 s) once the copy ends; no decoys; per-Radio pitch, Morse keying as the copy cue, 2.0 s copy.
- [Campaign save contents](issues/11-campaign-save.md): three Campaign slots + a separate settings slot via the existing Platform save API; Campaign = per-Shift best stars/score + owned Requisition ids in named, tolerantly loaded sections (Star bank and unlocks derived); autosave at Shift end and on each purchase, no mid-Shift save; Title → Continue/Play → Slot picker (delete only); stale ids dropped, unparseable slots marked Unreadable.
- [Spec chunking & hand-off order](issues/13-spec-chunking-order.md): six vertical specs handed off 1 Room & bootstrap → 2 Core loop → 3 Operators, Swap & AI → 4 Faults & Shift 2 → 5 Meta, with 6 Procedural audio any time after 2; paste-ready `/to-spec` packets (boundary, reads, settles, exit) live on the ticket; ImGui and the new [Synth stream behind Platform](../synth-stream/issues/01-synth-stream-platform.md) land on `main` before any slice implementation; remaining fog is settled inside the spec that introduces it.
- [Shift 1 tutorial](issues/14-shift1-tutorial.md): Briefing card (auto until the slot has a result, then a Shift picker button) + data-driven Hints (settings toggle, all Shifts, retire after one use, route verbs, Dash, Swap); Shift 1 opens in **Orientation**: Window, schedule and gauge frozen until the first Clear of a pinned unlosable Ticket, then the AI Operator clocks on; skipped once the slot has 1★, always held with 2+ humans.

## Not yet specified

Nothing left as fog. Data schema, screens & HUD, the audio palette, the Game Target bootstrap and the debug panels are
each settled inside the spec that introduces them (see the packets on
[Spec chunking & hand-off order](issues/13-spec-chunking-order.md)). The Shift 1 tutorial and greybox readability
graduated into their own tickets.

## Out of scope

- Shifts 3–10 and their content: Power Panel, Cipher Rotors, Supply Rack, Antenna, Codebook, Reel-to-Reel, Darkroom,
  Two-Key; blackouts, storms, jamming, saboteur, blast doors, hotline and pneumatic tube. Later efforts.
- Human co-op (split keyboard or gamepads): deferred to slice 2. Slice 1 is single player + AI Operator, but the
  Operator model must not block adding human players later.
- Gamepad support: deferred to slice 2 to keep slice 1 small. Findings stay valid (see the Gamepad research
  ticket), and the per-Operator binding layer should not assume keyboard-only.
- Online co-op (the design shouldn't rule it out, but it isn't specced here).
- Final art, palette and lighting polish beyond greybox.
- VIP tickets: their hook is a fussy dispatch type, which needs a second route ([Tickets, window & scoring](issues/05-tickets-window-scoring.md)).
- Consumable requisitions and start-of-Shift boosts (slice requisitions are star-bought unlocks).
- Endless mode, environmental-hazard and saboteur expansions (GDD §9).
- Renaming the engine's `*.level.json` / Level concept (planned separately).
- DEFCON (the Shift 10 escalation clock, wall board and tint): a finale mechanic, not a mission select; the Destination now names the Shift picker instead. Shift 10 notes kept on the ticket ([DEFCON's role in a two-Shift slice](issues/09-defcon-in-slice.md)).
- Overdrive: it reads as a boost, and boosts are out ([Decoder Faults & the Oil Can](issues/06-condition-heat-overdrive.md)).
- The Cooling Vent, core temperature and thermal runaway: the Vent doesn't fit the theme for now; a possible future idea ([Decoder Faults & the Oil Can](issues/06-condition-heat-overdrive.md)).
- A Condition meter and wear stages, the Loose Tube, Blown Fuse and Stuck Valve Faults, the Coolant Canister and the Supply Rack: slice Faults are random, not wear-driven ([Decoder Faults & the Oil Can](issues/06-condition-heat-overdrive.md)).
- Decoy Radios and their tell (garbled tone + flicker): dropped from the slice, a possible later Shift gimmick ([Radio presets & spectrum read](issues/12-radio-tuning-prototype.md)); backlogged as [Decoy Radio as a Shift gimmick](../backlog/issues/04-decoy-radio-gimmick.md).
- Backlogged outside this map: [Near-miss blast knocks power cables loose](../backlog/issues/01-near-miss-power-cables.md), [Rattling tell before a Fault](../backlog/issues/02-fault-rattling-tell.md) and [Ping a Station to steer the AI Operator](../backlog/issues/03-ping-station-steer-ai.md).
- Playtest telemetry: tuning uses the ImGui panels and Flecs Explorer only ([Spec chunking & hand-off order](issues/13-spec-chunking-order.md)).
