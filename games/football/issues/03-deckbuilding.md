# Phase 3 — Deckbuilding Content

**Status**: ⬜ Not Started
**Depends on**: Phase 2 (Core Drive Loop) — ✅ Complete **and Core Gate passed**
**Spec**: [spec](../spec.md)

## Goal

Add the Balatro pillar's breadth: a variety of stackable modifiers (with synergies and conditions), a
distinct trick-play category, and a draft/playbook screen where the player builds their deck before a drive.
Content + one new screen wired directly into the validated loop — no gyms.

## Desired Final State (Guiding Light)

> **When Phase 3 is done, two different drafts play like two different games.** A player opens a draft
> screen, assembles a capped playbook from a pool (including trick plays), stacks a few modifiers, and heads
> into the drive with an odds profile *they authored*. Stacked and conditional modifiers combine legibly —
> the player can point at the active-modifiers panel and explain why 3rd-and-short is suddenly a near-lock.
> Trick plays read as a real gamble: sometimes a huge swing, sometimes a disaster. The decision space has
> gone from "which of a fixed hand" to "what deck do I want to be."

**Measures of success** (the GATE measures these):
- **Synergy is real, not additive noise**: at least a couple of modifier combinations produce an
  "aha, these work together" moment — decisions between builds feel meaningful.
- **Conditions are transparent**: a conditional modifier's trigger (down/field-zone) is visible in the
  preview / active-modifiers panel *before* it fires.
- **Trick plays feel distinct**: their high-variance nature is obvious in the odds and in the outcome swing.
- **Draft creates identity**: a short-pass-heavy book and a trick-heavy book demonstrably play differently.
- **No regressions**: everything the Core Gate validated still holds.

**Explicitly NOT in scope**: multi-quarter flow, scoreboard/clock, game-over/loss cadence (Phase 4), polish.

## Tasks

> Guidelines, not a script — most of this is **JSON content** plus a few handlers and one screen. The single
> structural rule that carries over: modifier stacking must compose **deterministically** through the shared
> `ComputeOdds`. Use **flecs-systems** for any new system.

1. **Expand the modifier effect registry** — `systems/modifier_effects.cpp`.
   - Grow to **6–10 handlers** spanning three shapes: additive (`boost_category`, `first_down_bonus`),
     conditional/gated (`redzone_bonus`, `reroll_on_down` — down/field-zone triggers), and swingy stacking
     (`double_or_nothing`).
   - Guarantee stacked application is order-defined and reproducible; make sure conditions read `DriveState`
     correctly.
   - *Done when:* each handler is one function + one registry entry, and stacking two of them is deterministic.

2. **Author modifier JSON (breadth)** — `assets/data/modifiers/*.json` (6–10 total).
   - Cover the new effect keys; include several `condition`-gated (`down==3`, `fieldZone==redzone`) and
     `stackable: true` entries. Vary magnitudes so combinations matter.
   - *Done when:* all load (loader counts reflect them) with no parse warnings.

3. **Add trick plays** — `assets/data/plays/*.json` (2–3, `category: trick`).
   - e.g. `flea_flicker`, `hail_mary`, `fake_punt`: wide `yardBuckets` + elevated `turnoverChance` — big
     upside, real downside.
   - *Done when:* trick plays load and their odds visibly show high variance in the preview.

4. **Active-modifiers / synergy panel** — extend `ui/football_hud.cpp`.
   - Show currently-active modifiers and any triggered conditions/synergies during a drive, so stacking is
     legible in context.
   - *Done when:* a player can read, at a glance, which modifiers are live and why.

5. **Trick-play styling in the hand** — extend `ui/hand_view.cpp`.
   - Surface trick plays distinctly (badge/color) and reflect their high-variance odds in the preview.
   - *Done when:* trick plays are unmistakable in the hand and their risk is self-evident.

6. **Draft / playbook screen** — `ui/draft_view.{hpp,cpp}`.
   - Present a pool of plays; the player picks a capped playbook (cap from `football_constants.hpp`) and cuts
     others, then attaches available modifiers to the playbook/plays.
   - The confirmed `Playbook` feeds the drive loop's `Hand` generation.
   - *Done when:* the player can build a legal, capped book + attach modifiers, and confirm it.

7. **Insert a Draft step before a drive** — `football_scene.cpp` (+ `football_module.cpp` if needed).
   - Add a `Draft` step that runs before the drive and seeds the in-drive hand from the drafted playbook.
     (Full between-drive/quarter cadence is finalized in Phase 4.)
   - *Done when:* draft → drive flows in one session and the drafted book clearly shapes the hand.

## Verification Checklist

- [ ] Desktop and WASM builds succeed.
- [ ] 6–10 modifiers and 2–3 trick plays load; loader counts reflect them; no parse warnings.
- [ ] Stacked modifiers compose deterministically; conditional modifiers apply only when their condition
      holds, and that's visible in the preview / active-modifiers panel.
- [ ] Trick plays are visually distinct and show their high-variance odds.
- [ ] The draft screen builds a capped playbook + attaches modifiers; confirming seeds the in-drive hand.
- [ ] Two different drafts (short-pass-heavy vs. trick-heavy) produce visibly different drives.
- [ ] No regressions in the Phase 2 preview/resolution/drive/result popup.

## 🛑 GATE

Measured against the Desired Final State. Ask the human:
- **Do stacked modifiers create interesting synergy decisions (not just bigger numbers)?**
- **Are conditional modifiers' triggers clear in the preview / active-modifiers panel?**
- **Do trick plays feel like a genuine high-risk / high-reward gamble?**
- **Does drafting give a drive a meaningful identity, and is the draft/attach flow intuitive?**
- **Any blocking issues?** (true = fix now; false = note in Feedback)
- **Additional feedback?** (free text)

## Git Checkpoint

**Stage** (only these): `games/football/src/game/football/systems/modifier_effects.*`,
`games/football/src/game/football/ui/football_hud.cpp`, `games/football/src/game/football/ui/hand_view.cpp`,
`games/football/src/game/football/ui/draft_view.*`, `games/football/src/game/football_scene.cpp`,
`games/football/src/game/football/football_module.cpp` (if changed), `assets/data/modifiers/**`, `assets/data/plays/**`.
Verify with `git diff --cached --stat`.

**Commit**: `feat: deckbuilding content (modifier variety, trick plays, draft screen)`
(+ `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`)

## Feedback

_(none yet)_
