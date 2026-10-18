# Requisitions for a two-Shift slice

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: resolved
Blocked by: 05, 06

## Question

What can be bought between Shifts in the slice, with what currency, at what cost, and does any choice carry real trade-offs? Candidates from the GDD: faster crank, slower wear, oil can stock, wider radio lock zone. Settle the upgrade list for Shifts 1–2, whether upgrades are per-Operator or room-wide, whether Shifts can be replayed to farm currency, and how an upgrade expresses itself in data (modifier on a station stat) so new ones need no C++.

Context from [Tickets, window & scoring for Shifts 1–2](05-tickets-window-scoring.md): currency is **stars**. A Shift
pays its newly earned stars (best per Shift minus already banked) into a bank, so replays can't farm. Requisitions are
**unlocks** that deduct stars (e.g. 2 each); no consumables or start-of-Shift boosts in the slice. The slice pool is at
most 6 stars, so size costs and catalogue to it.

Context from [Decoder Faults & the Oil Can in Shift 2](06-condition-heat-overdrive.md): there's no wear meter and no oil
stock. The Oil Can is a single reusable tool. "Slower wear" and "oil can stock" become levers on the Fault model
instead (a longer cooldown range, Jam/Seize weights, a faster Mash or oil Hold, a second Oil Can or one closer to the Decoder).

## Answer

Resolved 2026-10-17 (grilling). All numbers are starting values and live in data.

**Purpose.** With at most 6 stars in the slice there is no real economy to balance. Requisitions exist to prove the loop
end to end: earn stars, browse the catalogue, buy, feel it in play, all driven by data. Balance waits for the full campaign.

**Room-wide only.** A Requisition changes Stations, tools and tuning for the whole room, never one Operator. This keeps
"an Operator is the same entity whoever drives it" intact and needs no per-Operator UI. AI Operators get the upgrade too
and work at their profile's rate (~90%) of the *upgraded* value.

**Data shape.** A catalogue entry = id, name, one-line blurb (says when it helps, e.g. "Shift 2: Faults"), star cost,
optional availability condition (unused by the base set, reserved for later content), and a **Modifier**. Owning a
Requisition switches its Modifier on in every Shift. Layering order: Shift base data → Shift Modifiers → owned
Requisitions → dev toggle (which can force a Requisition off for A/B tests). Upgrades are relative, so the Modifier
override schema gains a **`scale`** op beside **`set`**, so a Requisition never clobbers a Shift's own value. Exact
schema stays in the map's Data schema fog. New Requisitions need a JSON entry and no C++.

**Base catalogue** (always on offer, nothing to unlock; 6★ total = the whole slice pool):

| Requisition | Effect | Helps in | Cost |
|---|---|---|---|
| Geared Crank | Decoder crank ~2.5 s → ~2.0 s | Shift 1 + 2 | 1★ |
| Long-Throw Antenna | Radio Hold-to-copy 2.0 s → 1.5 s (the GDD's "wider lock zone", recast: no needle in the slice) | Shift 1 + 2 | 1★ |
| Mil-Spec Tubes | Fault cooldown 25–45 s → 35–60 s of Job run time | Shift 2 | 2★ |
| Field Repair Kit | Jam Mash 3× → 2×; Seize oil Hold ~1.5 s → ~1.0 s | Shift 2 | 2★ |

Dropped: a second Oil Can (one is enough; the walk to it is Shift 2's pressure). Possible later data-only entries:
Quick-release Lever (Jam only), Direction Finder (greys out decoy spikes), Auto-Teleprinter (`telex_autostamp` sold as
an unlock).

**Trade-offs.** Pure upgrades, no sidegrades. Scarcity is the decision: at most 3★ before Shift 2 buys the two cheap
helpers, or one Fault upgrade plus one helper. In Shift 2 the choice is rarer Faults vs faster fixes.

**Ownership.** Permanent and always on, replays included (replays get easier, which is fine because stars can't be
farmed). No respec, refund or player-facing toggle; only the dev toggle can switch one off.

**Currency and gating.** **Stars earned** (best per Shift, summed) never goes down and drives progression: Shift 2
opens when Shift 1 is passed with at least 1★. The **Star bank** = stars earned − stars spent, and only it pays for
Requisitions, so spending can never lock a Shift.

**When.** Only outside a Shift, on a Requisitions screen reached from the results screen and from the Shift picker.
Layout of that screen stays in the Screens & HUD fog.

Glossary: added **Requisition** and **Star bank**.
