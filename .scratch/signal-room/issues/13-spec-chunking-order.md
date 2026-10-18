# Spec chunking & hand-off order

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:grilling
Type: grilling
Status: open
Blocked by:

## Question

The core-loop decisions are all resolved (route, Operators & Swap, layout, tickets & scoring, Faults, AI, Requisitions,
Campaign save, Radios & the Antenna Coupler). Which implementable specs do they roll up into, and in what order are
those specs handed off? For example: Game Target bootstrap, core loop, Operators & AI, meta (Requisitions + save +
Shift picker), audio. For each spec, settle its boundary, which decisions and fog patches it owns, and what it depends
on. This includes whether engine work (the synth stream, the ImGui debug UI) must land first. Also decide which of the
remaining fog (data schema, screens & HUD, Shift 1 tutorial, audio palette, greybox readability, playtest loop)
becomes its own ticket and which is settled inside a spec as it's written.
