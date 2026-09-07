# Phase 5 — Polish

**Status**: ⬜ Not Started
**Depends on**: Phase 4 (Full 4-Quarter Game) — ✅ Complete required
**Spec**: [spec](../spec.md)

## Goal

The prototype already plays fully; make it *feel* complete enough to evaluate the whole concept: a real sound
set, an animated field token, arcade result juice, imgui styling, and balance tuning. Polish only — no new
mechanics.

## Desired Final State (Guiding Light)

> **When Phase 5 is done, the prototype reads as an arcade football roguelike, not a spreadsheet.** Calling a
> play sounds and feels punchy; the ball token slides between yard lines; a first down, touchdown, and
> turnover each land with distinct juice; the cards, odds bars, and scoreboard look cohesive and are easier
> to read than before. The easy/hard curve feels tuned rather than arbitrary, and a displayed seed lets a run
> be reproduced. It's the version you'd hand to someone and say "tell me if this is fun" without apologizing
> for the presentation.

**Measures of success** (the GATE measures these):
- **Feel**: outcomes are legible *and* satisfying — audio + juice reinforce what happened.
- **Cohesion**: the UI looks intentional; the odds preview is more legible than the Phase 2 version.
- **Reproducibility**: the shown seed replays a run.
- **Balance**: across a few games, easy vs hard feels fair-but-distinct; no obviously dominant play/modifier.
- **No mechanics changed** — this phase only makes the existing game read and feel better.

**Explicitly out of scope / future work**: momentum meter, music, save/load, settings menu, real opponent
AI, cross-run meta-progression. Note any of these in Feedback rather than building them.

## Tasks

> Guidelines, not a script — polish is judgment-driven; spend effort where the game currently reads worst.
> The fixed target is the Desired Final State, not a specific asset list. Do this only after Phase 4's gate
> confirmed the game plays well (polish on top of broken mechanics is wasted).

1. **Asset pipeline setup** — `Tools/`.
   - Copy the game-plan skill templates (`~/.copilot/skills/game-plan/templates/*`), `npm install jsfxr`,
     `pip install trimesh numpy pillow`; point each script's `OUTPUT_DIR` at `assets/audio/sfx` and
     `assets/textures`.
   - *Done when:* the scripts run and emit into the right asset dirs.

2. **Generate SFX set** — `Tools/generate_sounds.js`.
   - Preset-base-with-overrides pattern. Cover: play-call, tiered yard-gain, first-down, touchdown fanfare,
     turnover, trick-play sting → `assets/audio/sfx/`.
   - *Done when:* each key game event has a distinct, fitting sound.

3. **Generate simple textures** — `Tools/generate_textures.py`.
   - Field/card/HUD textures (grass stripes, card backing, red-zone tint) → `assets/textures/`.
   - *Done when:* the field strip and cards use real textures instead of flat rectangles.

4. **Wire audio to outcomes** — map each outcome → SFX (reuse the `ui::SetDefaultClickSound`-style pattern);
   replace the Phase 2 placeholder `yard_gain`/`turnover` with polished versions.
   - *Done when:* every outcome plays its mapped clip with no missing-clip warnings.

5. **Animate the field token** — `ui/football_hud.cpp`: tween the token between yard lines on resolve
   (the only per-frame animation).
   - *Done when:* the token slides smoothly rather than snapping.

6. **Result juice** — `ui/result_popup.cpp` + HUD: big-number yardage popups, first-down / TD flourishes,
   red-zone emphasis, a flash/shake on turnover.
   - *Done when:* the four key outcomes (gain, first down, TD, turnover) each read instantly and feel weighty.

7. **imgui styling** — cohesive arcade palette for cards, odds bars, scoreboard, draft screen; sharpen odds
   legibility.
   - *Done when:* the UI looks intentional and the preview is easier to read than in Phase 2.

8. **Balance tuning + seeded-run display** — iterate `plays/modifiers/scenarios` JSON for a satisfying
   easy/hard curve; surface the run **seed** on-screen and confirm it reproduces a run.
   - *Done when:* balance feels reasonable over a few games and the seed reproduces deterministically.

9. **Full playtest** — walk all 11 Prototype Boundary items in both scenarios; confirm feel end to end.
   - *Done when:* a clean playthrough in each scenario with no glaring feel or readability gaps.

## Verification Checklist

- [ ] Desktop and WASM builds succeed.
- [ ] Asset scripts run; files land in `assets/audio/sfx/` and `assets/textures/`.
- [ ] Every outcome plays its mapped SFX; no missing-clip warnings.
- [ ] The field token animates smoothly between yard lines.
- [ ] Result popups / flourishes read clearly and match outcomes (gain, first down, TD, turnover, trick).
- [ ] UI is cohesively styled; odds preview is more legible than before.
- [ ] Displayed seed reproduces a run; balance feels reasonable across easy/hard.
- [ ] Full playtest: all 11 Prototype Boundary items hold in both scenarios.

## 🛑 GATE

Measured against the Desired Final State. Ask the human:
- **Does the game feel complete enough to evaluate the concept (audio, juice, styling)?**
- **Is the easy/hard balance satisfying after tuning?**
- **Does the seeded run reproduce correctly?**
- **Any blocking issues?** (true = fix now; false = note in Feedback)
- **Additional feedback?** (free text — note remaining future-work items here.)

## Git Checkpoint

**Stage** (only these): `Tools/**`, `assets/audio/sfx/**`, `assets/textures/**`,
`games/football/src/game/football/ui/**` (touched), `assets/data/**` (balance edits). Verify with `git diff --cached --stat`.

**Commit**: `feat: football polish (audio, juice, animation, styling, balance)`
(+ `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`)

## Feedback

_(none yet)_
