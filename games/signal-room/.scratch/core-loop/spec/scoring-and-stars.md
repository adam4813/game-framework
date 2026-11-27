# Spec 2 · Scoring & stars

Part of [Signal Room spec 2: Core loop](../spec.md). Value, band share, the Handler Chain, Lost Intercept, par and
stars.

## User stories

48. As a player, I want a Cleared Ticket to score its tier's base (NORMAL 20, HIGH 30, FLASH 40), so that urgent
    Tickets are worth more.
49. As a player, I want a Ticket cleared in the green band to score in full, amber 75% and red 50%, so that speed pays.
50. As a player, I want consecutive Clears to build a Handler Chain (×1 → ×1.25 → ×1.5 → ×1.75 → ×2), so that a clean
    run is rewarded.
51. As a player, I want only a Lost Intercept to break the chain (a wasted transmission doesn't), so that the chain is
    about not dropping Tickets.
52. As a player, I want a Lost Intercept to cost 10 points, with my score never below 0, so that losses sting but never
    go negative.
53. As a player, I want stars at 40%, 65% and 90% of par, where par is the sum of base scores of the Tickets that went
    On Air this attempt, so that the bar is fair whatever the schedule rolled.
54. As a designer, I want a Shift to be able to override the star fractions or give absolute thresholds, so that set
    piece Shifts can be tuned on their own.

## Implementation decisions

- **Score state** is a world singleton (reflected): `score`, `chain_step` (index into `handler_chain`), `par`,
  `cleared`, `lost`, `best_chain_step`. It resets at each attempt start and at Reload.
- **On Clear:** `value = round(base × band share × handler_chain[chain_step])`, where the band is read at stamp time.
  Add it to `score`, then advance `chain_step` (capped at the last step). The multiplier applies to the Ticket that
  is cleared, then steps up, so the first Clear scores ×1.
- **On Lost Intercept:** `score = max(0, score − lost_intercept_penalty)` and `chain_step = 0`.
- **Par** adds a Ticket's tier `base` when it goes On Air (the pinned Orientation Ticket counts). It is final at the
  hard stop.
- **Stars** are computed at the hard stop: thresholds are `fractions × par` (from the Shift's `stars.fractions`, else
  the Ticket rules default), or the Shift's `stars.absolute` [1★, 2★, 3★] if given (a load error if both are given).
  The score can pass par (the chain does that); stars cap at 3.
- **Events:** `ScoreChanged(delta, reason)` and `ChainChanged(step)` for the HUD and later audio.
- Stars earned here are shown, not banked. The Star bank and save are spec 5.
