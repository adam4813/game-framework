# Football: game design document

Football is a roguelike deckbuilder in a football jersey. You call the plays for one drive down the field: draw a
hand of play and modifier cards each down, play modifiers to reshape the odds, then pick a play and hike it. The
outcome rolls from the play's yardage table. Three first-down segments and a boss drive make a run, with a shop
between segments that grows a deck you keep for the whole run. A run takes a few minutes.

This document describes the game as built. The original plan (four quarters, easy and hard scenarios, a draft
screen) and its phase tickets are in the history of PR #55 (`games/football/spec.md`, `games/football/issues/`).
The build departs from that plan, so the plan isn't a guide to the current game.

## Design pillars

- **Every down is a decision.** A play card shows its average yards, turnover chance and flag chance with your
  modifiers already applied. You choose against legible odds, not a coin flip.
- **The deck is the strategy.** All skill is in what you buy and when you spend a modifier. There is no in-play
  action or timing input; plays resolve on their own.
- **Short and complete.** One run is one drive. There is no meta-progression beyond the high score.

## Glossary

| Term | Meaning |
|---|---|
| **Segment** | One leg of the drive: four downs to reach a yardage marker. A run is three normal segments and the boss drive. |
| **Boss drive** | The last segment. It scores a touchdown or a field goal instead of a first down. |
| **Down** | One play. A segment has four. |
| **Play card** | A callable play with a weighted yardage table, a turnover chance and a flag chance. |
| **Basic play** | Basic Run and Basic Pass. Free and shown every down beside the drawn hand, so you can always move the ball. Never in the deck or the shop. |
| **Modifier card** | A card played onto the hand to change the odds of the play you hike this down. A **utility**, **buff** or **debuff**. |
| **Trick play** | A high-variance play category (Hail Mary). |
| **Flag** | A penalty: the play is negated and loses 10 yards. |
| **Overage** | Yards gained past a segment's marker. Capped at 5 for points and coins. |

## Flow

```text
Title ──Play──▶ Play select ──Hike──▶ Resolve ──Continue──┬─▶ next down ──▶ Play select
  ▲                  ▲                                     │
  │                  │                                     └─▶ Segment end ──┬─▶ Shop ──Next Segment / Face the Boss──┐
  │                  └───────────────────────────────────────────────────────┼──────────────────────────────────────┘
  │                                                                          └─▶ (after the boss) Results
  └──────────────────────────────── Quit to Title ◀── Results ──Play Again──▶ Play select (new run)
```

- **Title:** the game's name, the best score, and a Play button.
- **Play select:** the hand, the always-available basic plays, the HUD and the distance bar. Play any modifiers,
  select a play, then Hike. View Deck opens an overlay listing the deck with card counts.
- **Resolve:** a short top-down animation of the play (about 1.25 s), then the result: yards gained, TURNOVER!, or
  PENALTY FLAG!, and Continue.
- **Segment end:** the segment's result and the coins earned. FIRST DOWN! (with the bonus points) or STOPPED SHORT;
  on the boss drive, TOUCHDOWN!, FIELD GOAL or DRIVE STALLED.
- **Shop:** three cards for sale between segments. Buy any you can afford, then Next Segment (or Face the Boss).
- **Results:** FINAL WHISTLE, the final score, "New High Score!" or the best score, first downs converted and the
  boss result. Play Again starts a new run in place; Quit to Title goes back.

The phases are tags on a flow entity (`phase::PlaySelect`, `Resolve`, `SegmentEnd`, `Shop`, `Results`). Each
phase's entry logic runs in an `OnAdd` observer, and the scene rebuilds its UI when the flow entity carries
`RebuildUITag`.

## Controls

Mouse only. Every action is a button: select or use a card, Hike, Continue, Buy, Next Segment, View Deck.

## The run

- **Segments:** three normal segments with a 10-yard marker, then the boss drive. Each segment has 4 downs.
- **A down:** the hand is redrawn (3 cards) and shown with Basic Run and Basic Pass. Playing a modifier applies its
  effect to this down and discards it. Selecting a play reveals the Hike button.
- **Resolving a play:** the turnover chance is rolled first, then the flag chance, then a weighted pick from the
  play's yardage table, plus any bonus yards. A turnover gains 0 yards. A flag costs 10 yards. The segment's yards
  never drop below 0.
- **Ending a segment:** a segment ends on a turnover, on reaching its marker, or after the 4th down.
- **The boss drive:** reach 25 yards for a touchdown, or at least 15 for a field goal. The run ends after it.

## Scoring and coins

| Result | Points |
|---|---|
| First down | Overage past the marker, capped at 5 |
| Stopped short or turnover | 0 |
| Boss touchdown | 50 + overage, capped at 5 |
| Boss field goal | 20 |
| Boss drive stalled | 0 |

- **Coins:** each segment earns its yards gained, capped at the marker + 5 (so up to 15 on a normal segment). A run
  starts with 0 coins. Cards cost 7–14.
- **High score:** the best final score is saved to the `football_high_score` save slot (a file on desktop,
  `localStorage` on the web) and shown on the title.

## The deck

- **Starting deck:** Run Dive ×2, Off Tackle, Short Pass ×2, Slant, Deep Pass, Screen, QB Sneak, Pay Off Ref,
  Playbook Boost (11 cards).
- **Cycle:** one draw pile for the whole run, shuffled at the start. The previous hand goes to the discard each
  down, and the discard is shuffled back in when the draw pile runs out.
- **Shop:** three random cards from every play and modifier except the basic plays. A bought card joins the deck
  and is shuffled into the draw pile, so it can come up this run.

## Plays

Defined in `assets/data/plays/` (one JSON file each). `yards` is a weighted table of `[yards, weight]` pairs.

| Play | Category | Yards (weight) | Avg | Turnover | Flag | Cost |
|---|---|---|---|---|---|---|
| Basic Run | run | −1 (1), 2 (3), 4 (3), 6 (1) | 2.9 | 2% | 0% | free |
| Basic Pass | pass | 0 (3), 4 (3), 7 (2), 10 (1) | 4.0 | 5% | 0% | free |
| QB Sneak | run | 1 (3), 2 (3), 3 (1) | 1.7 | 1% | 0% | 7 |
| Run Dive | run | −1 (1), 2 (3), 4 (3), 6 (2) | 3.2 | 2% | 0% | 8 |
| Short Pass | pass | 0 (2), 4 (3), 6 (2), 9 (1) | 4.1 | 4% | 0% | 9 |
| Screen | pass | −3 (1), 2 (2), 7 (2), 14 (1) | 4.8 | 5% | 5% | 9 |
| Off Tackle | run | −2 (1), 3 (2), 5 (3), 8 (2), 12 (1) | 5.2 | 3% | 8% | 10 |
| Slant | pass | 0 (1), 5 (2), 8 (3), 12 (2) | 7.3 | 6% | 6% | 11 |
| Deep Pass | pass | 0 (3), 12 (2), 20 (2), 30 (1) | 11.8 | 12% | 10% | 13 |
| Hail Mary | trick | 0 (6), 40 (1), 60 (1) | 12.5 | 25% | 15% | 14 |

## Modifiers

Defined in `assets/data/modifiers/`. `effect` selects how a card changes the hand (`FoldModifier` in
`football_logic.cpp`). Effects stack within a down.

| Modifier | Category | Effect | Cost |
|---|---|---|---|
| Clean Game | buff | −10% flag chance | 7 |
| Cool Head | buff | −8% turnover chance | 8 |
| Pay Off Ref | utility | No flag this down | 9 |
| Playbook Boost | buff | +5 yards | 9 |
| Hail Mary Juice | debuff | +12 yards, +15% turnover chance | 10 |
| Sticky Gloves | utility | No turnover this down | 11 |

## Presentation

- **Screen:** 1280×720. The HUD runs along the top (the segment, the down, yards against the marker, score and
  coins). A vertical distance bar on the left shows the segments up to GOAL.
- **Cards:** a row of play and modifier cards. A play card shows its category and its odds after the down's
  modifiers (average yards, turnover %, flag %). An "Active this down" line lists the modifiers played.
- **Resolve animation:** a top-down field strip. A run sends the carrier upfield behind a lead blocker while a
  defender closes for the tackle. A pass or trick play throws to a receiver; on a turnover the defender undercuts the
  catch.
- **Art:** low-resolution, dithered N64-style textures generated by `tools/make_textures.py`: field, panel, button,
  scoreboard, menu background and wood.
- **Audio:** only the engine's default button click.

## Technical notes

- **Game Target:** everything lives under `games/football/` (ADR-0006). The engine provides the scenes, the UI, the
  save module, the registries (`engine::core::Registry<T>`) and the seeded `engine::ecs::RngState`.
- **Data-driven:** plays and modifiers are JSON, loaded into `PlayRegistry` and `ModifierRegistry` when the scene
  loads. A new play or modifier with an existing effect needs no code; a new effect needs a branch in
  `FoldModifier` and in the odds math.
- **Tuning** lives in `src/game/football/football_constants.hpp` and the JSON.
- **Odds:** `EffectivePlayStats` (the card preview) and `ResolvePlay` (the roll) apply the same modifier math, but
  each computes it separately.
- **Randomness:** draws, shuffles, shop offers and resolution all use `RngState`. Runs aren't seeded yet: the
  stream starts from the engine's default seed at launch and carries on across runs, so the first run after every
  launch is the same.
- **Reflection:** `FootballState` and `HighScore` are reflected, so the Flecs Remote API can read and set them (for
  example `FootballState.coins`). The scene's identity tag is `FootballSceneTag`, so `switch-scene Football` works.
- **Assets:** the game's own assets are in `games/football/assets/`, layered over the shared root `assets/` at build
  time.
- **Tests:** none yet.

## Open work

- **Playtest and polish** (`issues/05-polish.md`): sound for outcomes, a seed per run shown on screen, and a balance
  pass, checked by a human playtest.
- **Shared odds path:** the preview and the roll should call one function so they can't drift apart.
- **Content:** only one trick play (Hail Mary). Modifiers have no conditions (for example, "on 3rd down").
- **Tests:** a scene-flow test like crabnado's, plus tests for `ResolvePlay`, `DrawHand` and segment scoring.
- **Not built from the original plan:** easy and hard scenarios, field-position odds, quarters and a clock, and a
  real opponent.
