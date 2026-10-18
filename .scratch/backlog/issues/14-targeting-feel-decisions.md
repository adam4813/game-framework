# Targeting feel decisions: corners, cone measure and the pulse

Status: needs-info
Source: review of [04: Targeting & the Target brighten](../../signal-room-room-bootstrap/issues/04-targeting-brighten.md), 2026-10-18

Readings of spec 1's Target rule that ticket 04 settled one way and the designer should confirm. Each answer is a small
change in `games/signal-room/src/room/operator_targeting.cpp` or `target_brighten.cpp`, plus a test that only the
chosen reading passes.

## 1. Is an Operator off a Station's corner on its worked side?

Spec: "an in-service Station the Operator stands on a worked side of".

- **Now:** the Operator's centre must be past the side *and* level with it (within the footprint's span). Standing just
  off a corner, facing the machine diagonally, gives no Station, even when both sides meeting at that corner are worked.
- **Alternative:** level with the side within a margin (for example the Operator radius), so a corner stance counts.

## 2. What does the cone measure to?

Spec: "the nearest workable thing within 1.2 m and ±50°".

- **Now:** distance and angle both go to the nearest point of each of the thing's cells. A thing counts when any one
  corner is inside the cone. `ASlightlyOffAimWithinTheConeTargetsTheNearestThing` relies on this (25° to the Counter's
  corner, 45° to its centre).
- **Alternative:** angle to the footprint centre and distance to the nearest point. This is stricter for big Stations.
- On a tie the last cell scanned wins. This only matters for exact ties.

## 3. Should the pulse depth be data?

Ticket: "rate and strength in data". Both are in data now, as is `brighten_lift` (how far the identity colour is
lightened first). The pulse swings between full strength and half of it (`kPulseDepth = 0.5`, a constant). Should it
be a `targeting.brighten_depth` field?

## 4. Spec authoring note

The ticket's "facing the Telex from its unworked south side" test is impossible in the bunker Layout, because
CounterTelex stands directly south of the Telex. Ticket 04 used a fixture Layout
(`games/signal-room/tests/fixtures/telex_open_south.*`). Later tickets that name a Layout position should check it
against `bunker.layout.json`.

## Answer

_(designer: pick an option for 1–3, then set Status: ready-for-agent)_
