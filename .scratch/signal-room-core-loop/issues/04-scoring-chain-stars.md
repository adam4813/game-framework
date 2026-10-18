# 04: Scoring, the Handler Chain & stars

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [Scoring & stars](../spec/scoring-and-stars.md), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** Clears and losses now count. A Clear scores its tier's base × the band share at stamp time × the
Handler Chain, then steps the chain. A Lost Intercept costs 10 (floored at 0) and resets the chain. Par adds each
Ticket's base as it goes On Air, and at the hard stop stars are earned at 40 / 65 / 90% of par (or a Shift's own
thresholds). No HUD yet: the Room panel shows the numbers.

**Blocked by:** 02 (Stations, Transforms & Jobs).

**Status:** ready-for-agent

- [ ] A score singleton (`score`, `chain_step`, `par`, `cleared`, `lost`, `best_chain_step`, and `stars` once Ended),
      reflected, reset at attempt start and on Reload.
- [ ] Clear value `round(base × share × handler_chain[chain_step])`, chain steps after; Lost Intercept penalty and
      chain reset; floor 0; par on On Air; stars at the hard stop from the Shift's `stars.fractions`, the Ticket rules
      default, or the Shift's `stars.absolute` (both given is a load error).
- [ ] Events `ScoreChanged(delta, reason)` and `ChainChanged(step)`.
- [ ] Room panel: score, par, chain and stars readouts.
- [ ] Harness tests (drive Clears with the debug spawn + transmission path or the real route, whichever is cleaner;
      seed pinned):
  - a green NORMAL Clear on a fresh chain scores 20; the next Clear is ×1.25;
  - a red-band Clear at chain step 2 scores round(20 × 0.5 × 1.5) = 15;
  - the chain caps at ×2;
  - a Lost Intercept at score 5 leaves 0 and resets the chain; an unmatched transmission changes neither;
  - par sums the base of every Ticket that went On Air, including ones later Lost, and excludes Stand-by ones;
  - stars: 0.65 × par earns 2★; a Shift with `absolute` thresholds uses them.
- [ ] Runtime: desktop build; clear one Ticket and lose one, query the score singleton. No asserts.

## Comments
