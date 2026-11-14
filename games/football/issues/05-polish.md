# 05: Playtest and polish

Spec: [GDD](../GDD.md). Read: The run, Scoring and coins, Plays, Modifiers, Presentation.

**What to build:** The game plays start to finish but is silent, and its numbers are untested. Make outcomes land,
make a run reproducible, and tune the numbers, all checked by a human playtest. Polish only: no new mechanics.

Already done, from the original phase 5: generated N64-style textures (`tools/make_textures.py`), the top-down
resolve animation, and the textured UI.

- **Sound for outcomes.** Only the default button click plays today. Add generated sound effects for hiking a play,
  a yardage gain (tiered by size), a first down, a touchdown, a field goal, a turnover, a flag and a trick play.
  Write the generator in `games/football/tools/` (like crabnado's `make_sfx.py`) and put the clips in
  `games/football/assets/audio/`.
- **Feedback for the big moments.** First down, touchdown, turnover and flag each get a distinct flourish on the
  resolve and segment-end screens (a flash, shake or bigger headline), so each one reads instantly.
- **Seed each run, and show the seed.** `InitRun` doesn't seed `RngState` today. The stream starts from the engine's
  default seed at launch and carries on across runs, so the first run after every launch is the same. Pick a seed
  per run in `InitRun`, store it (crabnado keeps `RunState.seed`), seed `RngState` from it, and show it on screen
  (on the results screen at least). Starting a run with that seed must replay the same draws, shop offers and
  outcomes.
- **Balance pass.** Tune the plays, modifiers and constants (`football_constants.hpp`) over several runs. Check
  whether any play or modifier is clearly dominant, whether the shop economy (coins earned against card costs)
  offers a real choice each visit, and whether the boss drive's 25-yard touchdown is reachable without luck.

**Blocked by:** none.

**Status:** needs-human-playtest

- [ ] Every outcome in the list above plays its own sound, with no missing-clip warnings.
- [ ] First down, touchdown, turnover and flag each have a distinct visual flourish.
- [ ] Each run gets its own seed, it is shown, and the same seed replays a run exactly.
- [ ] Tuned values live in `football_constants.hpp` or the JSON, with a short comment on each changed value.
- [ ] Checked by playing several full runs on desktop, with final scores, boss results and anything that felt
      dominant or dead noted in `## Comments`.

## Comments

_(none yet)_
