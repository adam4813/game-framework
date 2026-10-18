# Requisitions for a two-Shift slice

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: open
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
