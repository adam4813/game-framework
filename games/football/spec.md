# Gridiron Roguelike — Implementation Plan

> Concept analysis produced by the **game-concept** skill; implementation plan produced by the
> **game-plan** skill and split into per-phase tickets under `issues/` for use with the **implement-phase**
> skill. **This file is the Football Game Target's spec** (`games/football/spec.md`), separate from the
> pinball `plan.md` at the repo root.
>
> The prototype as built departs from this plan: a run is three first-down segments plus a boss drive,
> with a shop between segments and a persistent deck, rather than four quarters with scenarios. See
> Cross-Session Handoff.

## Game Brief

**Concept**: A roguelike deckbuilder wearing a football jersey. You're a play-caller marching down the
field: draft a playbook, bolt on stackable modifiers, then call plays against shifting odds to score
touchdowns. Part N64-era arcade football nostalgia, part Balatro-style probability engineering.

**Genre**: Roguelike deckbuilder + Sports (football) — hybrid. All skill lives in pre-play decisions and
deck construction.
**Perspective**: Fixed 2D "hybrid" view — an abstract card/probability play interface with a light
top-down field strip where a ball token advances/retreats for spatial feedback. Not a 3D sim.

**The 3Cs**:
- **Character**: The player is a play-caller/coach — a disembodied strategist. The on-field "avatar" is
  the team's ball-position token that moves up/down the field strip. No directly controlled athlete.
- **Camera**: Fixed. A top-down field-position strip (progress) plus a card/hand UI region for play
  selection and modifiers. No follow-cam, no orbit.
- **Control**: Point-and-click / select. Draft plays, attach modifiers, pick a play from the current
  hand, confirm. **Pure selection → auto-resolve** — no in-play timing input.

**Core loop** (every 10–30 seconds):
- Read the situation (down & distance, field position, scenario odds).
- Pick a play from the current hand (with its attached modifiers).
- Confirm; the outcome **auto-resolves** from combined probabilities.
- Watch the result — ball token advances/retreats, yardage popup — then repeat until TD or turnover.

**Decisions** (every 1–2 minutes):
- Which plays to draft into the playbook; which to cut.
- Which stackable modifiers to attach, and how to build synergies.
- When to gamble on a high-variance **trick play** vs. play the percentages.
- Risk vs. safe given down, distance, field position, and the active scenario's odds curve.

**Monitors** (what the player watches):
- Field position (yard line / progress along the strip).
- Down & distance (4 downs, first-down marker).
- Scoreboard + quarter/clock.
- Odds preview for the selected play (success % / yardage range).
- Active modifiers & synergies currently in effect.
- Juicy result feedback (yardage popups, crowd/crunch reaction).

**Reacts to** (what happens TO the player):
- Turnovers — lose possession / lose ground per the scenario's fail configuration.
- Opponent touchdowns — a TD (by either side) ends the quarter.
- Scenario-driven probability shifts — field advantage/disadvantage changes the odds curve based on
  field position and down/distance.
- Variable, data-driven loss conditions per run/scenario.

**Between loops** (meta progression):
- Draft plays into the playbook (build the deck).
- Collect and attach stackable, Joker-like modifiers (odds boosts, conditional effects).
- Acquire trick plays (rare, high risk / high reward).
- Shape probability distributions before the next drive/quarter.

**Mood**: Arcade hype — big numbers, crunchy feedback, N64 energy. Atmosphere is **polish**, not core to
evaluating whether the concept is fun.

**Difficulty**: Data-driven per scenario. AI difficulty / probabilities scale with field position, down &
distance, and scenario configuration (field advantage/disadvantage). Prototype ships **two** scenarios:
an *easy* one (opponent merely scores, giving comeback room) and a *hard* one (failure = turnover).

**Prototype boundary**: In one session a player can play a single game — draft a small playbook, attach a
few modifiers (including at least one trick play), and call plays across downs to drive the field. They
will experience at least one touchdown and one turnover, watch odds shift with field position/scenario,
and play through the 4-quarter structure (a quarter ends when a TD is scored) across both the easy and
hard scenarios. The run definition is intentionally minimal and **structured to extend** later (real
opponent AI, longer seasons, meta-progression).

---

## Problem Statement

Fuse a sports-authentic football drive (downs, field position, turnovers, quarters) with a Balatro-style
deckbuilder (drafted plays + stackable modifiers + trick plays) so that calling plays against a
transparent, scenario-modulated odds engine produces meaningful, replayable decisions. The prototype must
prove that pre-play deck construction and play calling — with **no** in-play action layer — is fun on its
own, and that a data-driven scenario/odds engine can make field position and down/distance feel tense.

---

## Scope Assessment

**Tier**: Medium (trending Complex as content scales).

**Reasoning**: ~12 distinct mechanics with high interdependence — the odds engine, deckbuilding layer, and
drive rules must all work together before the core loop is testable, so there's real integration work.
Technical risk is concentrated in the **probability resolution engine** (combining base play distributions
+ modifiers + scenario + down/distance/field into a transparent, previewable outcome) and the
**data-driven scenario system**. Both football and deckbuilders are familiar genres, which lowers player-
comprehension risk, and **pure-selection / auto-resolve** removes action-layer and animation risk. The
smallest-testable run (4 quarters, TD ends a quarter, 2 scenarios) keeps the first playable tight, but the
concept grows toward Complex purely by adding content (more plays, modifiers, scenarios, real opponent AI).

**Iteration strategy**: Light iteration. Foundation → core resolution + drive loop → deckbuilding content →
full-game integration → polish. Human playtest gates after the core loop (the Core Gate) and after the full
game decide whether the fusion is fun.

---

## Concept Analysis

### Mechanics

- **Play selection** — pick a play from the current hand and confirm it.
- **Probability resolution** — auto-resolve a play into an outcome (yardage / turnover) from combined odds.
- **Odds preview** — compute and display success % / yardage range for the selected play, matching the engine.
- **Modifier attachment** — attach and stack Joker-like modifiers that shift a play's or playbook's odds.
- **Play drafting** — choose which plays go into the playbook between drives; cut others.
- **Trick play usage** — a distinct high-variance play category (big upside, real downside).
- **Field position tracking** — advance/retreat a ball token along a yard-line strip.
- **Down & distance tracking** — 4 downs, first-down markers, conversion logic.
- **Turnover handling** — possession/ground loss and fail-state resolution, configurable per scenario.
- **Scenario / AI difficulty modulation** — shift odds curves by field position, down/distance, and
  scenario config (field advantage/disadvantage); data-driven.
- **Scoring & quarter flow** — a TD (either side) ends the quarter; 4 quarters; scoreboard + clock.
- **Result feedback** — yardage popups, crowd/crunch reaction, big-number arcade juice.

### Systems (composed of mechanics)

- **Play resolution system** = play selection + probability resolution + odds preview + scenario modulation
- **Drive/field system** = field position tracking + down & distance tracking + turnover handling
- **Playbook / deckbuilding system** = play drafting + modifier attachment + trick play usage
- **Game flow system** = scoring & quarter flow + scenario config + variable loss conditions
- **Feedback / juice system** = result feedback + HUD monitors (polish)

### Priority Classification

**CORE (Phase 2)** — Play selection, Probability resolution, Odds preview, Field position tracking,
Down & distance tracking, Turnover handling, Scenario/AI difficulty modulation, **minimal** Modifier
attachment.
> Rationale: This is the core fun test — *"can I call plays against a transparent, scenario-shifting odds
> engine and march the ball down the field with meaningful turnover risk?"* Without the odds engine +
> drive loop there's nothing to evaluate. A **couple** of stackable modifiers must be present, because
> without any odds-shaping the decisions collapse into a hollow coin flip — the modifier layer is what
> makes calling plays a *decision* rather than a guess. Wrap it in the two scenarios (easy/hard) so field
> advantage is felt.

**SECONDARY (Phases 3–4)** — Full play drafting, Trick plays, Modifier synergies (stacking/conditional
interactions), full 4-quarter game flow + scoreboard/clock, between-drive draft.
> Rationale: These deepen the deckbuilder and provide the roguelike progression, but the core loop is
> evaluable without them. Build them only after the core odds/drive loop proves fun.

**NICE-TO-HAVE (Phase 5 / future)** — Result juice, animated field token, momentum/streak meter,
audio/music/theming, save/load & settings, cross-run meta-progression, real opponent offense/AI.
> Rationale: Atmosphere is polish for this genre, and the smallest run deliberately omits a real opponent.

### Complexity Profiles

- **Probability resolution** → **DEPTH** — the heart of the game. Must sophisticatedly combine base play
  distributions + modifiers + scenario + down/distance/field into a coherent, previewable outcome. A
  shallow coin-flip is not evaluable.
- **Odds preview** → **DEPTH** — must transparently and accurately mirror the resolution engine; player
  trust in the numbers is the whole point.
- **Scenario / AI difficulty modulation** → **DEPTH (logic) + BREADTH-light (content)** — the field-
  position → odds-curve logic is moderately deep; ship **2** data-driven scenarios (easy/hard) to prove it.
- **Modifier attachment** → **BREADTH** — need several (target ~6–10) distinct stackable modifiers so
  synergy decisions exist; each modifier is individually simple. Breadth **is** the test for the Balatro pillar.
- **Play drafting / playbook** → **BREADTH** — need a variety of plays (short/long, run/pass, plus trick)
  with distinct distributions so drafting creates real decisions.
- **Trick play usage** → **BREADTH-light** — a few high-variance options to test the risk/reward category.
- **Play selection** → **DEPTH-light** — one clean, readable interaction; clarity over variety.
- **Field position + down & distance tracking** → **DEPTH-light** — one correct implementation of drive
  rules; no variety needed.
- **Turnover handling** → **DEPTH-light** — correct, config-driven fail-state resolution.

---

## Iteration Map

| Phase | Delivers | Core mechanics | Gate |
|-------|----------|----------------|------|
| **1 Foundation** | Runnable Football scene, data structs, JSON loader + registries, constants, title wiring | (scaffolding) | compiles & runs |
| **2 Core Drive Loop** | Single-drive loop: read odds → call play → auto-resolve → advance/retreat → TD/turnover, 2 scenarios, 2 modifiers | Play selection, Probability resolution, Odds preview, Field position, Down & distance, Turnover, Scenario modulation, minimal Modifier attachment | ⚠️ **Core Gate** |
| **3 Deckbuilding Content** | Modifier variety (6–10) + synergies, trick plays, draft/playbook screen | Play drafting, Modifier attachment (breadth), Modifier synergies, Trick plays | human playtest |
| **4 Full 4-Quarter Game** | 4-quarter game (TD ends quarter), scoreboard/clock, between-drive draft, loss conditions, difficulty ramp | Scoring & quarter flow, variable loss conditions | reaches Prototype Boundary |
| **5 Polish** | Sound (jsfxr), animated field token, result juice, imgui styling, balance tuning, seeded-run display | Result feedback + HUD juice | full playtest |

**Rationale**: The odds engine + drive loop is the core fun test and must be validated before any
deckbuilding depth is built. A *minimal* modifier layer ships in Phase 2 (2 modifiers) because without any
odds-shaping the play call is a hollow coin flip — but the modifier *variety* and trick plays (the breadth
pillar) wait for Phase 3 after the core is proven. The 4-quarter game flow and draft screen are Phases 3–4
because a single drive is enough to evaluate "is calling plays against the odds fun?"

**No gyms.** The game is 2–3 fixed UI scenes with little that can be evaluated in isolation, so each phase
implements *and* integrates its slice directly and is gated by a single human playtest.

---

## Design Decisions

**Art style**: Fixed 2D, imgui-driven interface + a Platform-drawn top-down field strip. Placeholder
rectangles/text until Polish. Arcade "big-number" feedback is the visual identity.

**Engine conventions** (per `AGENTS.md` / `copilot-instructions.md`):
- **Flecs 4.x module**: All football systems/components live in a single `FootballModule`
  (`world.import<game::football::FootballModule>()`). Use the **flecs-systems** skill when writing systems.
- **Data-driven**: Plays, modifiers, and scenarios are **JSON files** under `assets/data/`, parsed at
  scene load with **nlohmann-json** (already a vcpkg dependency, first use in this repo). Adding a play or
  modifier = a JSON entry + (for a genuinely new effect) one handler, zero-to-minimal other C++ changes.
- **Tuning constants**: `football_constants.hpp` holds `constexpr` values (field length=100, downs=4,
  quarters=4, hand size, etc.) — never scattered magic numbers.
- **Patterns**:
  - **Registry** (string-id lookup singletons): `PlayRegistry`, `ModifierRegistry`, `ScenarioRegistry`.
  - **Strategy/Handler** for modifier effects: an effect-type → handler registry. New effect = new handler
    + registry entry.
  - **State machine** for game phase: `PlaySelect → Resolving → Result → (NextDown | QuarterEnd | GameOver)`.
  - **Event-driven**: play resolution is a discrete event (a `PlayConfirmed` event/observer), not per-frame
    polling. Continuous bits (field-token tween, clock) stay per-frame.
- **Singletons** via `world.set/get/get_mut<T>()`: `DriveState`, `GameState`, `Playbook`, `OddsResult`,
  `RngState`.
- **Determinism**: seeded `RngState` singleton so runs are reproducible.
- **Platform abstraction**: field-strip drawing and any 2D primitives go through the `Platform` interface —
  never call Raylib directly from game logic.
- **Scene**: new `SceneId::Football`, registered in `game.cpp`, activated from the title scene. Systems
  tagged `.add<scene::GameScene>()` + `.add<ecs::Pausable>()` where appropriate. The pinball scenes remain.
- **Style**: `snake_case` files, `PascalCase` types/functions, `camelCase` locals/fields, tabs, one concern
  per file, early returns.

**Data strategy** — schema sketches:
- **PlayDef**: `id`, `name`, `category` (run/pass/trick), `tags` (short/long/deep…), `baseSuccess`,
  `yardBuckets` (weighted `{yards, weight}` list), `turnoverChance`.
- **ModifierDef**: `id`, `name`, `effect` (handler key, e.g. `add_success`, `boost_category`,
  `reduce_turnover`, `reroll_on_down`), `params`, optional `condition` (e.g. `down==3`, `fieldZone==redzone`),
  `stackable`.
- **ScenarioDef**: `id`, `name`, `oddsCurve` (success/turnover deltas keyed by field zone & down),
  `lossCondition` (`turnover` | `opponentScore` | `clock`), `opponentPace`. Two ship: `sandlot_easy`,
  `blitz_hard`.

---

## Folder Structure

```
games/football/src/game/
  football_scene.hpp/.cpp          # SceneId::Football — pipeline, load registries, build UI, wire systems
  football/
    football_module.hpp/.cpp       # Flecs module: register components + systems
    football_constants.hpp         # constexpr tuning (field length, downs, quarters, hand size)
    components/
      data_defs.hpp                # PlayDef, ModifierDef, ScenarioDef (plain data)
      drive_components.hpp         # DriveState, GameState (+ Phase enum) singletons
      playbook_components.hpp      # Playbook, Hand, AttachedModifier
      odds_components.hpp          # OutcomeDistribution, OddsResult, RngState
    data/
      data_loader.hpp/.cpp         # JSON (nlohmann) -> defs; populates registries via Platform asset dir
      registries.hpp               # PlayRegistry / ModifierRegistry / ScenarioRegistry singletons
    systems/
      odds_system.hpp/.cpp         # odds engine (pure compute): play + modifiers + scenario + drive
      resolution_system.hpp/.cpp   # RNG sample outcome on PlayConfirmed
      drive_system.hpp/.cpp        # yardage apply, down/distance, first down, TD, turnover
      game_flow_system.hpp/.cpp    # quarter/score/clock, phase transitions, loss condition
      modifier_effects.hpp/.cpp    # Strategy handler registry for modifier effects
    ui/
      football_hud.hpp/.cpp        # field strip + down/distance + scoreboard/clock + odds preview
      hand_view.hpp/.cpp           # play cards + selection + confirm
      draft_view.hpp/.cpp          # between-drive draft + modifier attach
      result_popup.hpp/.cpp        # yardage popup / arcade juice

assets/data/
  plays/*.json
  modifiers/*.json
  scenarios/*.json                 # sandlot_easy.json, blitz_hard.json

Tools/                             # asset-gen scripts (Phase 5): generate_sounds.js, etc.
```

CMake `GLOB_RECURSE` auto-discovers new `.cpp` files — no CMake edits needed for new sources.

---

## Prototype Boundary (MVP Scope)

When the prototype is done (end of Phase 4), a player can, in a single session:

1. Launch, and from the title choose to start a Football game and pick a **scenario** (easy / hard).
2. **Draft** a small playbook from a pool of plays, including at least one **trick play**.
3. **Attach** at least one **modifier** to shape their odds.
4. See a **hand** of plays, each with an **odds preview** (success %, yardage range, turnover %) that
   correctly reflects modifiers + scenario + down/distance/field position.
5. **Call a play**; watch it **auto-resolve**; the ball token advances/retreats with a **yardage popup**.
6. Convert **first downs** and score a **touchdown**.
7. Experience a **turnover / fail state** (hard scenario) and see ground/possession loss.
8. See odds **visibly shift** with field position and scenario (field advantage vs. disadvantage).
9. Play through **4 quarters**, where a **TD ends the quarter**, with a **scoreboard + clock**.
10. Reach **game over** via the scenario's configured **loss condition** and see the final score.
11. **Difficulty ramp**: the hard scenario / later quarters present tougher odds than the easy scenario /
    early quarters — the curve is demonstrably felt.

The plan stops when every item above is playable. Real opponent AI, longer seasons, shops, and cross-run
meta-progression are explicitly **future work**.

---

## Implementation Progress

Detailed, executable steps live in per-phase tickets under `issues/`. Use the **implement-phase** skill to run
the next `⬜ Not Started` phase. **This game's root plan is `games/football/spec.md`** (not `plan.md`, which
holds the separate pinball prototype) — point the skill at this file.

**Gyms are intentionally omitted.** The game is 2–3 fixed UI scenes with little that can be evaluated in
isolation, so each phase implements *and* integrates its slice directly, gated by a single human playtest at
the end of the phase (the `🛑 GATE`). The Core Gate lives at the end of Phase 2.

### Guiding philosophy: guidelines, not prescription

The task lists in each phase file are **guidelines, not a rigid script.** AI code generation is not
deterministic and the engine will surprise us — deviate from the exact steps wherever the code demands a
better shape. What is **not** negotiable is the **Desired Final State** stated at the top of each phase: it
is the guiding light every task serves and the yardstick the GATE measures against. When a task and the
Desired Final State conflict, the Desired Final State wins; rewrite the task. Each task carries a
`Done when:` line so success is locally measurable, and each phase's Desired Final State rolls up toward the
game's **North Star** below.

> **North Star** — A five-minute session must let a player *feel a real decision*: read transparent odds,
> shape them with a drafted playbook + stacked modifiers, call a play against a scenario-modulated engine,
> and march (or lose) the ball down the field — with turnovers that sting and field position that matters.
> If the numbers aren't legible or the choice feels like a coin flip, the prototype has failed, no matter
> how much content exists. Every phase is judged by how much closer it brings us to that felt decision.

| # | Phase | File | Delivers | Depends on | Status |
|---|-------|------|----------|-----------|--------|
| 1 | Foundation | [issues/01-foundation.md](issues/01-foundation.md) | Data types, JSON loader, registries, constants, Football scene + title wiring | — | ⬜ Not Started |
| 2 | Core Drive Loop | [issues/02-core-drive-loop.md](issues/02-core-drive-loop.md) | Odds engine, resolution, drive rules, HUD + odds preview, hand, result popup, 2 scenarios, 2 modifiers | Phase 1 | ⬜ Not Started |
| 3 | Deckbuilding Content | [issues/03-deckbuilding.md](issues/03-deckbuilding.md) | Modifier variety (6–10) + synergies, trick plays, draft/playbook screen | Phase 2 (Core Gate passed) | ⬜ Not Started |
| 4 | Full 4-Quarter Game | [issues/04-full-game.md](issues/04-full-game.md) | 4 quarters (TD ends quarter), scoreboard/clock, between-drive draft, loss conditions, difficulty ramp | Phase 3 | ⬜ Not Started |
| 5 | Polish | [issues/05-polish.md](issues/05-polish.md) | Assets, audio wiring, token animation, result juice, imgui styling, balance tuning | Phase 4 | ⬜ Not Started |

The prototype is **complete** when Phase 4 passes (all Prototype Boundary items playable); Phase 5 is quality.

### Cross-Session Handoff

- **2026-08-31**: plan authored and split into per-phase tickets under `issues/`.
- **2026-09-09**: prototype built in one pass, not phase by phase: segment-based drive (three first-down
  segments + boss touchdown/field-goal drive), hand of play + modifier cards, shop between segments,
  persistent deck, resolve animation. The phase statuses above were not updated and don't track it.
- **2026-11-07**: moved out of the Sandbox into its own Game Target (`games/football/`), with a title scene
  and a persisted high score.
- **Next action**: reconcile the phase tickets with the built prototype before implementing further.

---

## Notes

**Extensibility** (the run is deliberately minimal, structured to grow):
- **Real opponent AI**: the opponent is currently a scoring placeholder in scenario config. Replace with a
  possession-swapping AI that runs its own drives — the drive/odds systems already support "possession".
- **More content**: plays, modifiers, scenarios are pure JSON — expand without C++ changes; genuinely new
  effects need only a new handler in `modifier_effects`.
- **Seasons / meta-progression**: wrap the single game in a run structure (multiple games, shops, rewards)
  above `GameState` without touching the drive loop.
- **New effect categories**: the Strategy handler registry is the single extension point; a new key +
  handler is the whole change.

**Performance**: 2D/imgui + event-driven resolution is trivial load; no physics stepping. Keep the odds
engine a pure function so it's cheap to call for previews. Field-token tween is the only per-frame animation.

**Risk watch**:
- **Odds transparency** is the make-or-break — if players can't read risk/reward, the core fails. Prioritize
  legible preview UI in Phase 2, not Polish.
- **Determinism**: keep all randomness in the seeded `RngState`; avoid ad-hoc `rand()` so runs reproduce.
- **Preview ↔ resolution parity**: the preview and the resolver must use the **same** odds computation path
  (share `ComputeOdds`) or trust breaks. Phase 2 verification samples many plays to confirm parity.

**Future work (out of MVP scope)**: momentum/streak meter, animated 3D-ish field, music, save/load,
settings menu, cross-run meta-progression, in-play decision beats (if the Core Gate ever calls for them).
