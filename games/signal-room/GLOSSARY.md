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
A physical thing an Operator carries, one at a time: a Raw Card, a Decoded Card, a Morse Tape. An intercept Item carries the frequency it was copied from.
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

**Preset**:
One of the Radio's jump buttons, each tuned to a spike on its spectrum. A Ticket names a frequency, not a Preset.
_Avoid_: channel, slot

**Modifier**:
A named set of data overrides that a Shift switches on as one of its gimmicks; the same mechanism drives developer A/B tests.
_Avoid_: variant, mutator, toggle
