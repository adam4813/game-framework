# Phase 4 — Full 4-Quarter Game

**Status**: ⬜ Not Started
**Depends on**: Phase 3 (Deckbuilding Content) — ✅ Complete required
**Spec**: [spec](../spec.md)

## Goal

Wrap the validated drive loop and deckbuilding into a complete game: 4 quarters where a TD ends the quarter,
a real scoreboard + clock, a between-drive draft/reward step, and the scenario's variable loss condition.
When this phase passes, **every Prototype Boundary item (1–11) is playable and the prototype is complete.**

## Desired Final State (Guiding Light)

> **When Phase 4 is done, a player sits down, picks easy or hard, and plays a whole game start to finish —
> and it holds together as a game, not a loop.** They draft, drive, score or turn it over, watch the quarter
> flip on a touchdown, see the scoreboard and clock move, adjust their deck between drives, and eventually
> hit a game-over with a final score. The hard scenario and later quarters bite harder than easy/early ones,
> and the player feels that ramp. Crucially: **all 11 Prototype Boundary items are checkable off in one
> sitting, in both scenarios.**

**Measures of success** (the GATE measures these):
- **Completeness**: a full 4-quarter game can be played to a definite end and restarted, in both scenarios.
- **Coherence**: draft ↔ drive ↔ quarter-end ↔ game-over transitions are clean; scoreboard/clock never
  desync from `GameState`.
- **Ramp is felt**: a player can tell hard from easy, and late from early, without reading the numbers.
- **The boundary is met**: every one of the 11 Prototype Boundary items is demonstrably playable.

**Explicitly NOT in scope**: polished audio/juice/animation/styling and final balance (all Phase 5). A
functional scoreboard and a readable-but-plain UI are enough here.

## Tasks

> Guidelines, not a script — this phase mostly *extends* `game_flow_system` and the HUD and *reuses* the
> Phase 3 draft screen. The fixed target is the 11-item boundary; wire whatever shape of state machine makes
> that true. Use **flecs-systems** for system changes.

1. **Full quarter/score flow** — `systems/game_flow_system.cpp`.
   - Run **4 quarters**; a TD (player or opponent-score placeholder) **ends the quarter**; advance the clock;
     update the scoreboard.
   - Define the full state machine: `Draft → Drive(PlaySelect/Resolving/Result) → QuarterEnd →
     (NextQuarter | GameOver)`; make transitions explicit and single-owner.
   - *Done when:* four quarters run in order, each ending on a TD, with score/clock updated.

2. **Real scoreboard + clock** — `ui/football_hud.cpp`.
   - Replace the placeholder scoreboard/clock with live `GameState` (quarter, home/away score, clock).
   - *Done when:* the HUD always reflects `GameState` exactly; no stale values across transitions.

3. **Between-drive draft / reward** — reuse `ui/draft_view` from Phase 3.
   - After a drive resolves (score or turnover), let the player adjust playbook/modifiers before the next
     drive; wire `Draft ↔ Drive ↔ QuarterEnd` in `football_scene.cpp`.
   - *Done when:* the deckbuilding loop closes — decisions between drives carry into the next drive.

4. **Variable loss conditions + game over** — `game_flow_system.cpp` + scene.
   - Apply each scenario's `lossCondition` (`opponentScore` = comeback room for easy; `turnover`/`clock` for
     hard). On game over, show the final score and offer restart (to Draft or Title).
   - *Done when:* each scenario ends by its configured condition and restart returns to a clean state.

5. **Difficulty ramp** — scenario `oddsCurve` scaling and/or per-quarter difficulty factor.
   - Make hard/late tougher than easy/early in a way that is *perceptible*, not merely numeric.
   - *Done when:* a blind player can rank easy-early vs hard-late by feel.

6. **Prototype Boundary pass** — walk items 1–11 from [the spec](../spec.md) end to end in both scenarios.
   - Close any gaps so each item is genuinely playable.
   - *Done when:* all 11 are checked off in a single sitting, in both scenarios.

## Verification Checklist

- [ ] Desktop and WASM builds succeed.
- [ ] A full game plays start→finish in **both** scenarios; drafting occurs between drives.
- [ ] Quarter transitions work; a TD (either side) ends the quarter; scoreboard/clock stay consistent.
- [ ] Game over triggers per each scenario's `lossCondition`; final score + restart shown.
- [ ] Difficulty ramp is perceptible (hard/late ≠ easy/early).
- [ ] No regressions: odds preview, modifiers, trick plays, draft, and the drive loop all still work.
- [ ] **Every Prototype Boundary item (1–11) is playable.**

## 🛑 GATE

Measured against the Desired Final State. Ask the human:
- **Can you complete a full 4-quarter game in both easy and hard?**
- **Do quarter/score/clock and the between-drive draft flow feel coherent?**
- **Is the difficulty ramp noticeable between easy/hard and early/late?**
- **Are ALL 11 Prototype Boundary items playable? (If not, which are missing?)**
- **Any blocking issues?** (true = fix now; false = note in Feedback)
- **Additional feedback?** (free text)

Passing this gate means the **prototype is complete**; Phase 5 (Polish) is quality-only.

## Git Checkpoint

**Stage** (only these): `games/football/src/game/football/systems/game_flow_system.cpp`,
`games/football/src/game/football/ui/football_hud.cpp`, `games/football/src/game/football_scene.cpp`,
`assets/data/scenarios/**` (if ramp tuning edited). Verify with `git diff --cached --stat`.

**Commit**: `feat: full 4-quarter game (scoreboard, clock, between-drive draft, loss conditions)`
(+ `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`)

## Feedback

_(none yet)_
