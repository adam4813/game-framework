# Phase 2 — Core Drive Loop

**Status**: ⬜ Not Started
**Depends on**: Phase 1 (Foundation) — ✅ Complete required
**Spec**: [spec](../spec.md)

## Goal

Deliver the **core fun test**: read the odds, call a play, watch it auto-resolve, and march (or lose) the
ball down the field — with meaningful turnover risk, across 2 scenarios, with 2 odds-shaping modifiers.
Implemented *and* integrated as one continuous single-drive loop. Ends with the **Core Gate**.

## Desired Final State (Guiding Light)

> **When Phase 2 is done, a player can sit down and, for five minutes, feel a genuine decision every single
> down.** They glance at a legible odds preview (success %, yard range, turnover %), understand *why* it says
> what it says (this play, these 2 modifiers, this scenario, this down & field position), pick, confirm, and
> watch the ball token move with a punchy result. Good field position and smart modifier use visibly improve
> the odds; a bad gamble on 4th-and-long can end the drive. Switching from `sandlot_easy` to `blitz_hard`
> makes the field *feel* more hostile. The same seed replays identically.

**Measures of success** (the Core Gate measures these):
- **Legibility**: a new player can predict, roughly, how risky a play is *before* confirming — the numbers,
  not luck, drive the choice.
- **Parity**: over ~100 sampled plays, realized outcomes match the previewed distribution (the preview and
  the resolver call the *same* `ComputeOdds`).
- **Consequence**: a turnover is felt as a real setback; a first down / TD is felt as real progress.
- **Modulation**: moving the ball into the red zone or switching scenarios changes the odds in a way the
  player can see and reason about.
- **The core is fun enough to want more content** — this is the literal pass/fail of the Core Gate.

**Explicitly NOT in scope**: drafting, >2 modifiers, trick plays, multi-quarter flow, scoreboard/clock,
polish audio/juice. A *fixed* hand + 2 preset modifiers is correct here.

## Tasks

> Guidelines, not a script. The odds engine must be a **pure function shared by preview and resolver** — that
> is the one non-negotiable structural rule (parity is a Desired-Final-State measure). Use **flecs-systems**
> for systems/observers; keep all randomness in the seeded `RngState`.

1. **Modifier effect handlers (Strategy registry)** — `systems/modifier_effects.{hpp,cpp}`.
   - Build an `effectKey → handler` registry. A handler receives the in-progress odds/distribution +
     `ModifierDef.params` + `DriveState` (to evaluate conditions) and mutates the odds.
   - Implement `add_success` (bump `baseSuccess`, optionally gated by a tag/`condition`) and `reduce_turnover`
     (scale `turnoverChance`).
   - Define a **deterministic application order** (e.g. registry/insertion order) so stacking is reproducible.
   - *Done when:* both handlers are registered and unit-callable; adding a handler is one function + one map entry.

2. **Odds engine (pure compute)** — `systems/odds_system.{hpp,cpp}`.
   - `ComputeOdds(const PlayDef&, span<const AttachedModifier>, const ScenarioDef&, const DriveState&) ->
     OddsResult`, building an `OutcomeDistribution` first, then summarizing.
   - Compose in a clear pipeline: base `yardBuckets`/`baseSuccess`/`turnoverChance` → apply modifier handlers
     → apply scenario `oddsCurve` deltas (field-zone × down) → derive `successPct`, `yardMin/Max/Expected`,
     `turnoverPct`.
   - Keep it **pure** (no globals, no RNG) so both preview and resolver can call it.
   - Register a system that refreshes `OddsResult` for the selected hand play (on selection-change event, or a
     light recompute during `Phase::PlaySelect`).
   - *Done when:* changing play/modifier/scenario/down/field position changes `OddsResult` sensibly and cheaply.

3. **Resolution system** — `systems/resolution_system.{hpp,cpp}`.
   - Observer on a `PlayConfirmed` event: rebuild the distribution via the shared `ComputeOdds` path, sample
     `RngState` → concrete `{yards, turnover}` → write `LastOutcome`; set `Phase::Resolving` then `Result`.
   - No ad-hoc `rand()`; only `RngState`.
   - *Done when:* confirming resolves exactly one outcome per event, reproducible under a fixed seed.

4. **Drive system** — `systems/drive_system.{hpp,cpp}`.
   - Apply `LastOutcome` to `DriveState`: advance/retreat `ballYard`; decrement `down`; on gaining
     `yardsToFirst` reset downs and move the chains; on `ballYard >= 100` register a TD; on running out of
     downs or a rolled turnover apply the scenario `lossCondition` (hard = possession/ground loss; easy =
     opponent-score placeholder).
   - Drive the `Phase` transitions from `Result` to the next state.
   - *Done when:* first down, TD, and turnover paths each produce the correct `DriveState`/`Phase` result.

5. **Minimal continuous game flow** — `systems/game_flow_system.{hpp,cpp}`.
   - Just enough to loop: run downs; when a drive ends (score or fail), **reset for a new drive** so play is
     continuous within one quarter. (Full 4-quarter flow is Phase 4.)
   - *Done when:* the player can run drive after drive without restarting the scene.

6. **HUD: field strip + down/distance + odds preview** — `ui/football_hud.{hpp,cpp}`.
   - Platform-drawn field strip with the token at `ballYard`; down & distance text; placeholder
     scoreboard/clock; and the **odds preview** panel bound to `OddsResult`.
   - Prioritize legibility of the odds (consider bars + numbers). This UI is the make-or-break for the gate.
   - *Done when:* the preview updates live with selection and clearly communicates success/yardage/turnover.

7. **Hand view** — `ui/hand_view.{hpp,cpp}`.
   - Render the fixed hand as selectable cards; selection updates the previewed `OddsResult`; a Confirm
     button emits `PlayConfirmed`.
   - *Done when:* selecting different plays visibly re-previews; confirm triggers exactly one resolution.

8. **Result popup** — `ui/result_popup.{hpp,cpp}`.
   - Show the yardage/turnover outcome with arcade emphasis; advance on click/timer back to `PlaySelect`.
   - *Done when:* every resolution surfaces a clear, dismissible result that names what happened.

9. **Scene wiring + scenario selection** — `football_scene.cpp` + `football_module.cpp`.
   - Seed a fixed hand and 2 attached modifiers; build HUD/hand/result-popup; register all systems.
   - Make the scenario selectable at scene start (default `blitz_hard`; a debug key toggles `sandlot_easy`)
     so both odds curves get exercised.
   - *Done when:* both scenarios are reachable and the whole loop runs from a cold scene load.

10. **Testing-feel SFX (placeholder)** — `assets/audio/sfx/{yard_gain,turnover}.wav`.
    - Generate cheap jsfxr placeholders (`pickupCoin`, `hitHurt`) and play on resolve — success-vs-fail feel
      can't be judged silently. (Polished set is Phase 5.)
    - *Done when:* a gain and a turnover sound audibly distinct on resolve.

## Verification Checklist

- [ ] Desktop and WASM builds succeed.
- [ ] Selecting a hand play updates the preview; the preview reflects modifiers + scenario + down/field.
- [ ] Confirm resolves: token moves, `LastOutcome` set, popup shows, sound plays.
- [ ] Parity: over ~100 samples, realized outcomes track the previewed distribution.
- [ ] First down resets downs/chains; TD resolves the drive; 4th-down fail / turnover applies scenario fail.
- [ ] Loop is continuous (new drive/hand after each ends); no stuck `Resolving`.
- [ ] Switching scenarios visibly changes the odds curve (field advantage/disadvantage felt).
- [ ] Fixed seed reproduces the same sequence of outcomes.

## 🛑 GATE — ⚠️ CORE GATE: Is the core fun?

Play 5+ minutes in both scenarios, then ask the human (measured against the Desired Final State):
- **Is reading the odds and calling a play satisfying?**
- **Would you want to keep doing this with more plays and modifiers (i.e. proceed to deckbuilding)?**
- **Does field position + turnover risk create real tension?**
- **Is the odds preview legible — can you feel the risk/reward before confirming?**
- **What ONE thing would most improve the feel?** (free text)
- **Any blocking issues?** (true = fix now; false = note in Feedback)

If **NO**: iterate here before Phase 3 — adjust odds transparency, popup timing, sound punch, token speed;
rethink the odds presentation (numbers vs. bars vs. tiers); or simplify buckets/down-distance if outcomes
feel random rather than decided. If the gate fails 2–3 times, revisit whether pure-selection auto-resolve is
the right core, or whether a single in-play decision beat is needed.

## Git Checkpoint

**Stage** (only these): `games/football/src/game/football/systems/**`, `games/football/src/game/football/ui/football_hud.*`,
`games/football/src/game/football/ui/hand_view.*`, `games/football/src/game/football/ui/result_popup.*`,
`games/football/src/game/football/football_module.cpp`, `games/football/src/game/football_scene.cpp`, `assets/data/**` (if edited),
`assets/audio/sfx/yard_gain.wav`, `assets/audio/sfx/turnover.wav`. Verify with `git diff --cached --stat`.

**Commit**: `feat: core drive loop (odds engine, resolution, drive, hud, hand)`
(+ `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`)

## Feedback

_(none yet)_
