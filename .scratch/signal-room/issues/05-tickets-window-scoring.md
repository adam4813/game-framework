# Tickets, window & scoring for Shifts 1–2

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: resolved
Blocked by: 01

## Question

Settle the ticket economy for Shifts 1–2: which urgency tiers appear (NORMAL / HIGH PRIORITY / FLASH; VIP?), timers and how they cap to the remaining window, spawn cadence and the max on the rack, window length per Shift, what happens to open tickets at window close, base scores, the Handler Chain multiplier, the lost-intercept penalty, star thresholds, and the gauge colour bands. Starting values only; they live in data. Also: what score/currency comes out of a Shift for requisitions to spend?

Context: [Routine Cable route & station verbs](01-routine-cable-route.md) is resolved. Items carry a frequency and match the most urgent open ticket with that frequency at the Telex; unmatched/stale items transmit for nothing (no extra penalty). Shift 2 tickets name one of the Radio's preset frequencies; some spikes are decoys. Decide how many distinct frequencies are live at once and how often tickets share one.

## Answer

Resolved 2026-10-17 (grilling). All numbers are starting values and live in data.

**Ticket.** The domain term stays **Ticket**. In the fiction a Ticket is a tasking from HQ ("orders from the boss"), and
UI text may play with that wording freely. Lifecycle: **Stand-by → On Air → Cleared / Lost Intercept**.

**Tiers** are data rows. The slice ships three; VIP is out of scope until a second route exists (its hook is a fussy
dispatch type).

| Tier | On Air timer | Base | Heads-up overfill | Shifts |
|---|---|---|---|---|
| NORMAL | 90 s | 20 | 0–30% | 1, 2 |
| HIGH PRIORITY | 65 s | 30 | 0–15% | 1 (a few), 2 |
| FLASH | 40 s | 40 | 0% | 2 |

**Heads-up as gauge overfill.** A Ticket may arrive with its gauge **above 100%**. The portion above 100% is Stand-by:
the Ticket is on the rack but its frequency is silent. When the gauge reaches 100% it goes On Air and the tier timer
runs. Overfill is per-entry / per-tier data.

**On-air rule (tightens the Radio rule in [Routine Cable route & station verbs](01-routine-cable-route.md)).** The Radio
only copies a frequency while an On Air Ticket names it; otherwise static, no lock. Decoy spikes are always on air.
This stops stockpiling cards (trivial in Shift 1's single frequency). A card whose Ticket expired after copying still
transmits for nothing, as before.

**Timers & window.** Window 4:00 for both Shifts. A Ticket's On Air timer is `min(tier timer, window left)` at the
moment it goes On Air. **Last Call:** nothing new goes On Air in the final 20 s. At 0:00 the Shift **hard-stops**: open
Tickets are unscored (no penalty, no chain effect) and Items in flight are discarded.

**Collection Schedule (per Shift).** Mostly random, with authored set pieces:
- A seeded generator: arrival interval range (Shift 1: 25–35 s, Shift 2: 15–25 s), tier weights, frequency weights, a
  minimum gap before the same frequency repeats.
- Optional **pinned** entries at fixed times (tutorial, set pieces).
- Seed is fresh per attempt; a dev can pin it for tests and A/B work.
- **Rack cap 5:** a due Ticket waits for a free slot.
- **Empty rack:** the remaining wait to the next Ticket is halved (not pulled forward instantly).

**Frequencies.** Shift 1: one. Shift 2: 5 Presets = 3 live frequencies + 2 decoys. Sharing a frequency between open
Tickets comes out of the generator's frequency weights; "most urgent wins" at the Telex.

**Gauge bands** (fraction of the On Air timer): green > 50%, amber 25–50%, red < 25% (flashing, ticking buzzer).

**Value = base × band share × Handler Chain.** Band at stamp time decides how much of the message was caught: green
100%, amber 75%, red 50% (lore: a late intercept missed part of the message). No completeness indicator on the card.

**Handler Chain.** ×1 → ×1.25 → ×1.5 → ×1.75 → ×2 (cap), one step per consecutive Cleared Ticket. Only a Lost
Intercept breaks it; unmatched transmissions are neutral. Resets each Shift.

**Lost Intercept.** −10, chain broken, siren. Shift score floors at 0.

**Stars.** Thresholds are fractions of **par** = sum of base scores of Tickets that went On Air this attempt (fixed at
window close): 1★ 40%, 2★ 65%, 3★ 90%. The Handler Chain can push score past par.
A Shift may override with absolute thresholds.

**Payout for requisitions.** Stars are the currency. A Shift pays its **newly earned** stars (best for that Shift minus
already banked) into a star bank, so replays can't farm. Requisitions are **unlocks** that cost stars (e.g. 2) and
deduct them; no consumables or start-of-Shift boosts in the slice. Costs and catalogue: [Requisitions for a two-Shift
slice](10-requisitions.md).

## Comments

- 2026-10-17: [Radio presets & spectrum read](12-radio-tuning-prototype.md): Shift 2's frequencies are now **3 Radios on 3
  frequencies, no decoys** (replaces "5 Presets = 3 live + 2 decoys"). The on-air rule is unchanged (a Radio is lit while an On Air
  Ticket names its frequency). The Antenna Coupler adds one rule: while an Operator is copying at one Radio, the others
  go dark. The generator's frequency weights now cover 3 frequencies.
  Arrival intervals are unchanged; copying is one at a time, ~2.4 s per card with the reset.
