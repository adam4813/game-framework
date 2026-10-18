# 06: In-play HUD (rack, clock, score)

Spec: [Signal Room spec 2: Core loop](../spec.md). Read: [In-play HUD](../spec/hud.md), [Tickets, Window & Collection Schedule](../spec/tickets-and-schedule.md) (Gauge and events only), [Testing decisions](../spec/testing.md).
Base branch: `feat/signal-room` (branch off it, land per `games/signal-room/AGENTS.md`)
Model: sonnet

**What to build:** The player can finally see the Shift. A rack of up to 5 Ticket cards across the top (callsign,
tier, frequency swatch and label, a gauge with its Stand-by overfill segment and its green / amber / red On Air band,
flashing in red). Cleared cards flash green and Lost ones red as they leave. A Window clock shows `m:ss` and marks
Last Call. The score and Handler Chain multiplier pop when they change. Built with the engine UI module; layout
numbers and strings in a HUD data file, colours from the palette.

**Blocked by:** 04 (Scoring, the Handler Chain & stars).

**Status:** ready-for-agent

- [ ] A HUD layout data file (anchor, margins, card size and spacing, font sizes, flash / pop timings, label strings)
      and palette tokens for bands, text and panels. Tier rows carry a display `label`.
- [ ] Ticket cards created on `TicketArrived`, removed after an exit flash on `TicketCleared` / `LostIntercept`,
      ordered by arrival. Gauge fill and band colour updated per frame; the Stand-by part drawn as a dim segment past
      100%.
- [ ] The clock shows `window_left` as `m:ss`, with a "LAST CALL" tag in Last Call. (Ticket 10 adds "HELD".)
- [ ] Score and `×chain` labels, with a scale pop on `ScoreChanged` / `ChainChanged`.
- [ ] HUD entities live under the Room scene's root and go away on Unload and Reload.
- [ ] Harness tests (query the UI entities, not pixels):
  - an arriving Ticket adds a card whose labels show its callsign, tier label and frequency;
  - a 30% overfill Ticket's gauge reports a Stand-by segment; once On Air it reports green, then amber, then red as it
    drains;
  - after a Clear, the card is gone once its flash time passes; the rack never shows more than 5 cards;
  - the clock label reads "4:00" at start and "3:30" after 30 s;
  - the score label shows the new score after a Clear.
- [ ] Runtime: desktop build; screenshot the HUD with a Stand-by Ticket, a red-band Ticket and the Last Call clock
      (use the panel's spawn and freeze). Load the WASM build once and check the HUD draws. No asserts.

## Comments
