# AI Operator behaviour

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: resolved
Blocked by: 01, 02, 06

## Question

How does an AI Operator decide what to do and how is it 'correct but less efficient'? Settle: how it picks the next job (utility scoring over open tickets/route steps/repairs? GOAP? BT? weigh the engine's AI module), how it avoids clashing with the human (claims/reservations on stations and items), the inefficiency knobs (reaction delay, suboptimal picks, slower cranking), how it continues the human's in-progress job after a Swap, whether the player can steer it (ping/command) or not, and how it handles repairs. Its tuning must live in data.

Context: [Routine Cable route & station verbs](01-routine-cable-route.md) is resolved. Work is item-driven Transforms with one Job per Station and station-owned progress (an AI can resume any paused Job); items match tickets by frequency at the Telex. The AI must pick Radio Presets by ticket frequency, avoid decoys and over-producing items no ticket wants, and hand off only via Counters.

Context from [Operators & Swap](02-operator-seats-coop-input.md): an AI Operator is any Operator no Seat holds. After Swap it first finishes the hand-off (Hold or carry, waiting at a busy Station), then picks its own job. Each AI Operator shows an intent icon for its target Station. Arriving cancels its action, with ~0.25 s coyote time on Holds.

Context from [Tickets, window & scoring for Shifts 1–2](05-tickets-window-scoring.md): Tickets go Stand-by (gauge
overfilled, frequency silent) → On Air; the Radio only copies a frequency with an On Air Ticket (decoys always on air).
Value falls with the gauge band (100/75/50%) and a Lost Intercept breaks the Handler Chain, so the AI's job pick should
weigh urgency and band, and may pre-position during Stand-by.

Context from [Decoder Faults & the Oil Can in Shift 2](06-condition-heat-overdrive.md): no heat and no Overdrive in the
slice. From Shift 2 the Decoder Faults at random while running a Job. A Jam (progress to 0) is cleared by Mashing
Interact 3×; a Seize (paused) needs the Oil Can, a reusable tool Item on the Counter at (0,8), held at the Decoder
~1.5 s. The AI must decide who fetches the can and whether to drop or park its carried Item to do it.

## Answer

Resolved 2026-10-17 (grilling).

**Brain: a Job board and a scorer.** Each time an AI Operator needs a job, it lists the candidates the world state
offers right now: copy frequency F at the Radio, pick up a Station's output, load an Item into a Station whose
Transform accepts it, crank the Decoder, stamp at the Telex, repair a Fault, or pre-position at the Radio. It scores
each one and runs the winner as a short fixed step list (pathfind on the Layout grid, then Interact, Hold or Mash).
The scorer is hand-rolled. The engine's `ai::UtilityAgent` scores a fixed list of named actions, and these candidates
are generated per Ticket, Station and Item, so it doesn't fit. When nothing scores, the AI **wanders** a little around
where it stands (radius in data).

**Score** (all weights in data) = kind weight (repair > deliver > decode/crank > copy > pre-position)
+ urgency (On Air gauge left, sooner is higher)
+ value at stake (tier base × current band share, plus a bonus when a Lost Intercept would break the Handler Chain)
− distance cost.
Work that finishes the closest-to-done Ticket wins naturally.

**Demand and decoys.** It copies frequency F only while open On Air Tickets for F outnumber the F-Items in flight
(carried, on Counters, locked in Stations). It never copies a decoy and never makes Items no Ticket wants. A Stand-by
Ticket whose gauge is near On Air (lead time in data) gives a low-score **pre-position** candidate: stand at the Radio
on that frequency's Preset and start copying when it goes On Air. Items the human parks on Counters are ordinary
candidates; passing via Counters is how the crew hands off.

**Claims (no clashes).** An AI claims the Station and Item its job targets. Other AI Operators skip claimed targets,
which matters once Rosters grow. Claims never block the human. If the human takes a claimed target, the AI's job is
invalid and it re-picks. The intent icon from [Operators & Swap](02-operator-seats-coop-input.md) shows the claim.

**Correct but less efficient** (all per profile):
- **Reaction delay** before every pick and re-pick (random within a range, starting ~0.4–0.8 s).
- **Walk** at ~90% of the Operator walk speed. It **never dashes**.
- **Job speed** ~90%, set per Station: Hold-to-copy, Keypunch and the Decoder crank all run at the AI's rate.
  The AI does crank the Decoder (crank is a normal candidate, scored like the rest).
- No score noise. When it picks, it picks the best candidate.

**Commitment and preemption.** The AI keeps its job until it is done or invalid (target taken by the human, Ticket
gone with no other match, Station Faulted under it). The only thing that pulls it off a valid job is a **Fault**,
after a grace delay that gives the human time to react:
- 3 s if the AI is mid-Hold or carrying, 1.5 s otherwise.
- +50% if the human's Operator was closer to the Faulted Station when it struck (measured once, at the strike).
- When the grace runs out, the AI takes the repair only if no Operator has started it (Mashing a Jam, or carrying
  the Oil Can for a Seize). Otherwise it carries on.

**Repairs.** A Jam: walk over and Mash. A Seize: if carrying, first load the Item into a Station that accepts it, else
park it on the nearest free Counter; fetch the Oil Can, Hold at the Decoder, then leave the can on the free Counter
nearest the Decoder. Afterwards it re-picks, often its parked Item. With several AIs, the best-scoring one (distance
counts) claims the repair.

**Steering.** None in the slice: Swap is how you steer, and the hand-off rules from
[Operators & Swap](02-operator-seats-coop-input.md) stand. A "ping a Station" command is backlogged
([Ping a Station to steer the AI Operator](../../backlog/issues/03-ping-station-steer-ai.md)).

**Tuning data: AI profiles.** All knobs (reaction delay range, walk %, job % per Station, score weights, pre-position
lead, Fault grace delays and the closer-human factor, wander radius) live in an **AI profile** JSON. Each Operator
names its profile as a property. A Shift or the dev toggle can point it at another profile file, so the slice can
A/B test profiles. No skill tiers ("rookie"/"veteran") for now. A debug view (tooling decided in the map's Playtest & tuning fog) shows each
AI's Job board with candidate scores, for tuning.

Glossary: added **Claim**.

## Comments

- 2026-10-17: [Radio presets & spectrum read](12-radio-tuning-prototype.md): the AI only copies at the Radio the Antenna
  Coupler has lit. Its unmet-demand count is now also what the coupler uses to pick a Radio. **Drop pre-positioning**
  for near-On-Air Stand-by: which Radio lights next is random. "Never copies a decoy" is moot (no decoys in the slice).
