# Signal Room

A frantic co-op time-management game: "Overcooked in a 1962 Cold War wiretap bunker." Builds on the engine vocabulary
in `docs/GLOSSARY.md`.

## Language

**Signal Room**:
The Cold War SIGINT time-management game built as its own Game Target.
_Avoid_: Signal Round, Signal Room 1962 (as an identifier)

**Shift**:
One campaign stage: a single night in the bunker with its own transmission window, set of stations in service, and
flaky machine. Shifts are numbered (Shift 1 "Night Shift Orientation", Shift 2 "Hot Tubes", …).
_Avoid_: level (reserved for the engine's Level), night, stage

**Operator**:
A character on the bunker floor who carries items and works stations. At any moment an Operator is controlled either
by a human player or by the AI; the Operator itself is the same either way.
_Avoid_: player (a player is a human; an Operator is the character), worker

**Roster**:
The ordered set of Operators a Shift puts on the floor. Its order decides which Operator a human starts on and the
order Swap cycles through.
_Avoid_: crew, team, party

**Seat**:
One human's place at the console: their controls and the one Operator they currently hold. An Operator held by no Seat
is an AI Operator.
_Avoid_: player (as a thing that holds an Operator), controller, slot

**AI Operator**:
An Operator no Seat holds. It pursues the work it judges correct, deliberately less efficiently than a skilled human,
so solo play still has a co-op crew.
_Avoid_: bot, NPC, companion

**Claim**:
An AI Operator's mark on the Station or Item it means to work next, so other AI Operators leave it alone. A Claim never
stops a human.
_Avoid_: reservation, lock

**Swap**:
A human moving their Seat to the next Operator in the Roster that no other Seat holds. The Operator left behind
becomes an AI Operator that finishes the action or carry the human was in the middle of, then picks its next job.
_Avoid_: switch, possess

### The floor

**Station**:
A machine on the bunker floor that Operators work. It runs at most one Job at a time and holds that Job's items until they are picked up.
_Avoid_: machine (in identifiers), workstation, island

**Decoder**:
The Station at the center of the room, on the rug, that turns Raw Cards into Decoded Cards. The fiction calls it the
Relay Mainframe.
_Avoid_: Mainframe (fiction only, not an identifier)

**Layout**:
The floorplan a Shift is played in: the walls, rug and where every Station and Counter stands. A Shift names its Layout
and which of its Stations are in service; the rest stand tarped. Several Shifts may share one Layout.
_Avoid_: level (reserved for the engine's Level), map, room (as an identifier)

**Tarped**:
A Station standing in the Layout but out of service this Shift, under a canvas tarp. It blocks movement like any
furniture but cannot be worked.
_Avoid_: disabled, locked, inactive

**Item**:
A physical thing an Operator carries, one at a time: a Raw Card, a Decoded Card, a Morse Tape, or a tool such as the Oil Can. An intercept Item carries the frequency it was copied from.
_Avoid_: ingredient, object, pickup

**Transform**:
A Station's rule turning a set of input Items into output Items through a verb (Instant, Tap, Hold, Walk-away). The route a ticket takes is the chain of Transforms, not a stored recipe.
_Avoid_: recipe, route recipe, conversion

**Job**:
One run of a Transform on a Station, from its inputs going in to its outputs being picked up. Its progress belongs to the Station, not to the Operator.
_Avoid_: task, process

**Counter**:
A surface where an Operator may set down one Item. Items are never set on the floor.
_Avoid_: shelf, table, drop spot

**Modifier**:
A named set of data overrides that a Shift switches on as one of its gimmicks; the same mechanism drives developer A/B tests.
_Avoid_: variant, mutator, toggle

### Faults

**Fault**:
A Station's own breakdown, striking at random while it runs a Job. It stops the Job until an Operator repairs it. A Station has at most one Fault at a time.
_Avoid_: breakdown (in identifiers), hazard, crisis (keep that for world-caused events)

**Jam**:
A Fault that throws away the Job's progress. Working the machine clears it, and the Job starts over.
_Avoid_: gear jam, card jam (both mean Jam)

**Seize**:
A Fault that freezes the Job where it is until the Oil Can is applied; the Job then carries on.
_Avoid_: dry, stuck, needs oil

**Oil Can**:
A reusable tool Item that repairs a Seize. It is never used up.
_Avoid_: oil, lubricant, consumable

### Tickets & scoring

**Window**:
The transmission window a Shift lasts: the stretch of the night enemy stations are on the air. When it closes the
Shift ends.
_Avoid_: timer, round, shift clock (as an identifier)

**Ticket**:
A tasking from HQ on the rack: a callsign and frequency to intercept, with a tier and a time gauge. It moves from
Stand-by to On Air, then ends Cleared or as a Lost Intercept.
_Avoid_: order (lore wording only), tasking (lore wording only), request

**Stand-by**:
A Ticket that has arrived but whose station is not yet transmitting; its frequency is silent.
_Avoid_: pending, queued

**On Air**:
A Ticket whose station is transmitting; only now does its frequency's Radio light up and can be copied.
_Avoid_: active, live

**Lost Intercept**:
A Ticket whose gauge ran out while On Air. A Ticket still open when the Window closes is not a Lost Intercept.
_Avoid_: expired order, failed ticket

**Handler Chain**:
High Command's growing confidence in the station: a multiplier built by consecutive Cleared Tickets and broken by a
Lost Intercept.
_Avoid_: combo, streak

**Collection Schedule**:
A Shift's plan for which Tickets arrive and when: mostly random within its limits, with some Tickets pinned to fixed
times.
_Avoid_: wave, spawn table

**Antenna Coupler**:
The relay that feeds the room's single antenna to one Radio at a time. Engaging a Radio to copy couples it, and every
other Radio goes dark until it disconnects.
_Avoid_: antenna switch, active radio

**Coupler reset**:
The short dark beat after the Antenna Coupler disconnects, before every Radio with traffic comes back online.
_Avoid_: cooldown

**Par**:
The sum of base scores of the Tickets that went On Air in an attempt; star thresholds are fractions of it.
_Avoid_: max score, target

**Requisition**:
A room-wide improvement bought with stars between Shifts. Once bought it is kept and applies to every Shift and every
Operator.
_Avoid_: upgrade, perk (as identifiers), unlock

**Star bank**:
The stars available to spend on Requisitions: every star earned across Shifts, minus those already spent.
_Avoid_: wallet, currency, points

**Campaign**:
One playthrough's progress across Shifts: best results per Shift and the Requisitions bought. Up to three Campaigns
exist side by side, each in its own save slot; settings are shared and sit outside them.
_Avoid_: profile, run, save (the save is where a Campaign is kept, not the Campaign)

### Onboarding

**Briefing**:
The card shown before a Shift that lays out its route and what's new. It shows on its own until the Campaign has a
result for that Shift, and can be reopened from the Shift picker.
_Avoid_: tutorial, intro, splash

**Hint**:
A prompt floating over the Station that is the next step for the human's own Operator, showing the key to press. Each
kind of Hint retires for the attempt once that human has done it. Hints can be switched off in settings.
_Avoid_: tooltip, tutorial prompt, callout

**Orientation**:
A Shift's held opening: the Window, the Collection Schedule and the first Ticket's gauge wait until the first Ticket is
Cleared, and the AI Operator clocks on then. A Shift opts in; it applies while the Campaign hasn't earned a star on that
Shift, or whenever more than one human is seated.
_Avoid_: warm-up, dry run, tutorial (Shift 1's name "Night Shift Orientation" is fiction, not this term)
