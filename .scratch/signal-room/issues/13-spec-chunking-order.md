# Spec chunking & hand-off order

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: resolved
Blocked by:

## Question

The core-loop decisions are all resolved (route, Operators & Swap, layout, tickets & scoring, Faults, AI, Requisitions,
Campaign save, Radios & the Antenna Coupler). Which implementable specs do they roll up into, and in what order are
those specs handed off? For example: Game Target bootstrap, core loop, Operators & AI, meta (Requisitions + save +
Shift picker), audio. For each spec, settle its boundary, which decisions and fog patches it owns, and what it depends
on. This includes whether engine work (the synth stream, the ImGui debug UI) must land first. Also decide which of the
remaining fog (data schema, screens & HUD, Shift 1 tutorial, audio palette, greybox readability, playtest loop)
becomes its own ticket and which is settled inside a spec as it's written.

## Answer

Resolved 2026-10-17 (grilling).

**Six vertical specs.** Each ends in something playable and builds on a game that already runs. Hand-off order:
**1 → 2 → 3 → 4 → 5**, with **6** any time after 2. The user writes each spec with `/to-spec`, then `/to-tickets`,
outside the map. This ticket supplies the context for each one: the packets below.

**Engine first.** Both framework pieces land on `feat/signal-room` (the project's base branch, which never merges into
`main`) before any slice implementation starts:
- [Dear ImGui on Raylib behind a debug UI interface](../../imgui-debug-ui/issues/01-imgui-raylib-backend.md)
- [Synth stream behind Platform](../../synth-stream/issues/01-synth-stream-platform.md) (new; built from the audio
  research)

**Fog.** Two patches get their own tickets: [Shift 1 tutorial](14-shift1-tutorial.md) (grilling, gates spec 2) and
[Greybox readability & juice](15-greybox-readability.md) (prototype, gates spec 1). Every other patch is settled
inside the spec that introduces it. Later specs extend earlier schemas rather than redefining them. Telemetry is out
of scope.

### Spec 1: Room & bootstrap

- **In:** the `games/signal-room/` Game Target per ADR-0006 (CMake `add_subdirectory`, entry point, `tests/`, scenes).
  A Shift file names a Layout and its in-service Stations, and the rest are Tarped. The Layout file covers placements,
  footprints, worked sides, wall heights and spawn markers. One 13×9 Layout with 6 Counters. One keyboard Operator
  with walk, dash and per-Item carry speed. Hand-rolled 2D collision. Facing-cell targeting with cone fallback and the
  target highlight. Pick up and set down a placeholder Item on a Counter. The fixed whole-room iso camera. Greybox
  Station and Item reads.
- **Out:** Station verbs and Transforms, Tickets, Swap and AI, Faults, meta, audio.
- **Read:** `map.md`; issues 07, 02 (camera only), 15.
- **Settles:** the Layout and Shift→Layout schema; greybox shapes and colours (per 15).
- **Depends on:** both engine prerequisites landed; ticket 15 resolved.
- **Exit:** walk, dash and collide through the Shift 1 Layout among in-service and Tarped machines, and carry and set
  down an Item with the target highlight showing.
- **Prompt:** `Signal Room spec 1 (Room & bootstrap). Read .scratch/signal-room/map.md and issues 07, 02, 13, 15. Boundary as in issue 13's Spec 1 packet.`

### Spec 2: Core loop (Shift 1 solo)

- **In:** Items and item-driven Transforms. Fixed-frequency Radios with their display, traffic lamp and the antenna
  coupler (`coupler_reset`). Decoder (walk-away + crank) and Telex (place + stamp). Ticket tiers, the gauge overfill
  (Stand-by → On Air) and the seeded Collection Schedule with pinned entries. Scoring, the Handler Chain, Lost
  Intercept, the hard stop at 4:00 and stars against par. The briefing card, results/stars screen and in-play HUD
  (rack, Window clock). Modifiers (`set` override + dev toggle; `keypunch`, `telex_autostamp`). Shift 1 onboarding
  (per 14): the Briefing card (always shown for now), the Hint rule table with route-verb and Dash Hints, and
  Orientation's freeze and pinned Ticket (always applied until spec 5). ImGui debug buttons: spawn a Ticket, freeze the Window.
- **Out:** Swap and AI, Faults, Requisitions and save, audio (silent or placeholder only).
- **Read:** `map.md`; issues 01, 05, 07, 12, 14, 15.
- **Settles:** schema for Stations/Transforms, Items, ticket tiers, the Collection Schedule, Radios in the Shift file,
  and the Modifier override schema (`set` + layering under the dev toggle); in-play HUD; briefing and results.
- **Depends on:** spec 1; ticket 14 resolved.
- **Exit:** one human clears Shift 1 to a stars screen.
- **Prompt:** `Signal Room spec 2 (Core loop). Read .scratch/signal-room/map.md and issues 01, 05, 07, 12, 13, 14, 15. Boundary as in issue 13's Spec 2 packet.`

### Spec 3: Operators, Swap & AI

- **In:** the Seat model, the per-Shift Roster (2), one-key instant Swap in Roster order, the left-behind Operator
  finishing its Hold or carry, and coyote time on arrival. A per-Operator action-binding layer that doesn't assume
  keyboard-only. The Seat ring, chevron and AI intent icons. The AI Job board and scorer, soft AI-only Claims,
  reaction delay and speed factors, and the swappable AI profile JSON. ImGui AI tuning panel and Job board view. The AI Operator clocks on when Orientation ends, and the Swap Hint
  (per 14).
- **Out:** AI Fault handling (spec 4), human co-op and gamepad (slice 2).
- **Read:** `map.md`; issues 02, 03 (binding layer only), 08, 14, 15.
- **Settles:** the AI profile schema and the Roster in the Shift file; the Seat ring, chevron and intent icons.
- **Depends on:** spec 2.
- **Exit:** a human and an AI Operator clear Shift 1 together, and Swap works mid-Hold and mid-carry.
- **Prompt:** `Signal Room spec 3 (Operators, Swap & AI). Read .scratch/signal-room/map.md and issues 02, 03, 08, 13, 14, 15. Boundary as in issue 13's Spec 3 packet.`

### Spec 4: Faults & Shift 2

- **In:** Decoder Faults (Jam: Mash 3× and progress reset; Seize: pause, Hold with the Oil Can). The hidden seeded
  run-time cooldown and the pinned Seize at ~20 s. The Oil Can on its Counter. Shift 2 content: 3 Radios, its tickets
  and Collection Schedule. AI Fault preemption (grace, closer-human bonus, the park → fetch Oil Can → leave it
  sequence). Fault Hints (per 14). ImGui buttons to force a Jam or Seize.
- **Out:** wear and Condition, other Fault kinds, the rattling tell (backlog).
- **Read:** `map.md`; issues 06, 08, 05, 07, 12, 14.
- **Settles:** the Fault data schema (cooldown range, kind weights, pinned first Fault); Fault HUD reads.
- **Depends on:** spec 3.
- **Exit:** Shift 2 is playable with Faults, the pinned Seize fires, and the AI handles Faults.
- **Prompt:** `Signal Room spec 4 (Faults & Shift 2). Read .scratch/signal-room/map.md and issues 05, 06, 07, 08, 12, 13, 14. Boundary as in issue 13's Spec 4 packet.`

### Spec 5: Meta

- **In:** the Requisition catalogue (base 4, star costs, Modifier wrapping, the `scale` op and layering order), the
  Star bank and earned-star gating. Three Campaign slots and a settings slot through the Platform save API, tolerant
  loading, and autosave at Shift end and on purchase. The Title (Continue / Play), Slot picker (delete confirm,
  Unreadable), Shift picker and Requisitions screen (reached from results and the Shift picker). The web
  click-to-unlock on Title. The Hints toggle in the settings slot, the Shift picker's Briefing button, and the
  slot-derived rules for Briefing auto-show and Orientation's 1★ check (per 14).
- **Out:** consumables and boosts, DEFCON, mid-Shift save.
- **Read:** `map.md`; issues 10, 11, 05, 09, 14.
- **Settles:** the Requisition catalogue schema, the `scale` op, and the Campaign and settings save documents; all
  meta screens.
- **Depends on:** spec 4 (Shift 2 must exist for the Shift picker and the Field Repair Kit).
- **Exit:** Title → Slot → Shift picker → play → results → Requisitions, with progress persisting across restarts.
- **Prompt:** `Signal Room spec 5 (Meta). Read .scratch/signal-room/map.md and issues 05, 09, 10, 11, 13, 14. Boundary as in issue 13's Spec 5 packet.`

### Spec 6: Procedural audio

- **In:** game hookup to the synth stream. The palette: per-Radio tone and Morse copy keying, the coupler-reset relay
  clunk and static sweep, relay chatter, Jam grind, Seize squeal, stamp thud, Fault alarm and ticket buzzer. Voice
  params in JSON. Events and state drive the voices, not per-frame polling.
- **Out:** the synth stream itself (framework ticket), AudioWorklet (deferred unless playtests show dropouts).
- **Read:** `map.md`; issues 04 (and docs/research/procedural-audio.md), 01, 06, 12.
- **Settles:** the audio palette and the voice param schema.
- **Depends on:** spec 2 and the synth stream. Sounds for Faults land once spec 4 exists.
- **Exit:** every palette sound is synthesised in play, on both desktop and web.
- **Prompt:** `Signal Room spec 6 (Procedural audio). Read .scratch/signal-room/map.md and issues 01, 04, 06, 12, 13 (and docs/research/procedural-audio.md). Boundary as in issue 13's Spec 6 packet.`

## Comments

- 2026-10-18: Spec 1 written: [Signal Room spec 1: Room & bootstrap](../../signal-room-room-bootstrap/spec.md) (`ready-for-agent`).
- 2026-10-18: Spec 2 written: [Signal Room spec 2: Core loop](../../signal-room-core-loop/spec.md) (`ready-for-agent`, 12 tickets).
