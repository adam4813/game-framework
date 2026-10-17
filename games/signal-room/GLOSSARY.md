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

**AI Operator**:
An Operator currently controlled by the AI. It pursues the work it judges correct, deliberately less efficiently than a
skilled human, so solo play still has a co-op crew.
_Avoid_: bot, NPC, companion

**Swap**:
In solo play, the human moving control from one Operator to another. The Operator left behind becomes an AI Operator
that finishes what the human was doing, then picks its next job.
_Avoid_: switch, possess
