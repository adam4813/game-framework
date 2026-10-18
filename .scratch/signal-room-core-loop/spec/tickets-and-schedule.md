# Spec 2 · Tickets, Window & Collection Schedule

Part of [Signal Room spec 2: Core loop](../spec.md). Ticket rules data, tiers, the gauge, Stand-by → On Air → Cleared
/ Lost Intercept, the Window, Last Call, the hard stop, the schedule and the seed.

## User stories

31. As a player, I want Tickets from HQ to arrive on a rack, each with a callsign, a frequency and a tier, so that I
    know what to intercept.
32. As a player, I want a Ticket's gauge to drain while it's On Air, with the tier deciding how long (NORMAL 90 s,
    HIGH PRIORITY 65 s, FLASH 40 s), so that urgent Tickets feel urgent.
33. As a player, I want some Tickets to arrive with their gauge overfilled above 100%, so that I get a heads-up before
    their frequency goes On Air.
34. As a player, I want a Stand-by Ticket's frequency to stay silent until its gauge drains to 100%, so that the
    heads-up can't be exploited.
35. As a player, I want a Ticket whose On Air gauge runs out to become a Lost Intercept and leave the rack, so that
    missing one has a clear consequence.
36. As a player, I want stamping a card at the Telex to clear the most urgent On Air Ticket of its frequency, so that I
    never have to say which Ticket a card is for.
37. As a player, I want a card for a frequency with no On Air Ticket to transmit for nothing (no score, no penalty), so
    that a stale card is just wasted time.
38. As a player, I want the Shift's Window to last 4:00, so that every Shift is a short, intense run.
39. As a player, I want a Ticket's On Air timer capped to the Window time left, so that nothing goes On Air that can't
    be finished.
40. As a player, I want nothing new to go On Air in the last 20 s (Last Call), so that the end isn't a trap.
41. As a player, I want the Shift to stop hard at 0:00, with open Tickets unscored and Items in flight discarded, so
    that the ending is clean.
42. As a player, I want the rack to hold at most 5 Tickets, with a due Ticket waiting for a free slot, so that the rack
    never overflows.
43. As a player, I want an empty rack to halve the wait to the next Ticket, so that I'm never idle for long.
44. As a designer, I want tiers to be data rows (timer, base score, overfill range), so that VIP or other tiers can be
    added later without C++.
45. As a designer, I want each Shift's Collection Schedule to be a seeded generator (arrival interval range, tier
    weights, frequency weights, a minimum gap before a frequency repeats) plus pinned entries at fixed times, so that
    play is varied but set pieces are authored.
46. As a designer, I want a fresh seed per attempt, and a way to pin it, so that play varies but tests and A/B runs
    repeat exactly.
47. As a designer, I want callsigns drawn from a data list, so that the flavour text is editable.

## Implementation decisions

- **Ticket rules** (new data file): `tiers` (id → `on_air_time`, `base`, `overfill` [min, max] as a fraction of
  `on_air_time`), gauge `bands` (green > 0.5, amber 0.25–0.5, red < 0.25 of the On Air timer) with their `share`
  (1.0 / 0.75 / 0.5), `handler_chain` steps ([1.0, 1.25, 1.5, 1.75, 2.0]), `lost_intercept_penalty` (10),
  `rack_cap` (5), `last_call` (20 s), `empty_rack_wait_factor` (0.5), default star `fractions` ([0.4, 0.65, 0.9]) and
  a `callsigns` pool. Tiers shipped: `normal` (90 s, 20, 0–0.30), `high` (65 s, 30, 0–0.15), `flash` (40 s, 40, 0).
- **Shift file** gains `window` (seconds, 240) and `schedule`:
  `interval` [min, max] s, `tier_weights` (tier → weight), `frequency_weights` (label → weight, every label must be on
  an in-service Radio), `min_repeat_gap` (s before the same frequency may arrive again; ignored when only one
  frequency is live), and `pinned` (list of `{at, tier, frequency, overfill, unlosable}`, `at` in Window seconds).
  Load errors name the bad entry.
- **Ticket entities** (reflected): callsign, tier, frequency, arrival order, `standby_left` (s), `on_air_total` and
  `on_air_left` (s), state **Stand-by → On Air → Cleared / Lost Intercept**, and an `unlosable` flag. Cleared and Lost
  Tickets leave the rack at once (the HUD may play a short exit); results keep their counts.
- **Gauge.** At arrival, `standby_left = overfill × tier on_air_time`, where overfill is drawn from the tier's range
  (or the pinned value). It drains in real seconds. On reaching 0 the Ticket goes On Air with
  `on_air_total = min(tier on_air_time, window left)`. The displayed gauge is `1 + standby_left / tier time` while on
  Stand-by and `on_air_left / on_air_total` while On Air. The band comes from the On Air fraction.
- **Matching.** On a transmission of frequency *f*, clear the On Air Ticket with frequency *f* and the least
  `on_air_left` (lowest arrival order on a tie). No match: nothing happens.
- **Shift clock** is a world singleton (reflected): `window_left`, `phase` (see [Shift flow](shift-flow.md)), the
  attempt's `seed`, and the next arrival timer. The Window, Ticket gauges and the schedule tick only in the Running
  phase, and not while the debug freeze is on.
- **Collection Schedule.** After each arrival, draw the next wait uniformly from `interval`. While the rack is empty
  the remaining wait drains at `1 / empty_rack_wait_factor` speed (twice as fast). A due Ticket that finds the rack
  full waits until a slot frees. Draw the tier by `tier_weights`, then the frequency by `frequency_weights` among those
  not used within `min_repeat_gap` (fall back to all if none qualify). Pinned entries arrive at their `at` regardless
  of the generator and don't reset its wait.
- **Last Call.** When `window_left ≤ last_call`, the schedule stops, and a Stand-by gauge stops draining at its On Air
  line, so it stays Stand-by.
- **Hard stop.** At `window_left = 0` the phase becomes Ended: open Tickets are left unscored (no penalty, no chain
  effect), Items in flight (carried, on Counters, inside Jobs) are destroyed, and Holds end.
- **Randomness** comes from the world `RngState`, seeded per attempt. A fresh seed is drawn at each attempt start
  unless the debug panel (or a test) pins one.
- **Events** for the audio and HUD: `TicketArrived`, `TicketOnAir`, `TicketCleared`, `LostIntercept`, `LastCall`,
  `ShiftEnded`.
