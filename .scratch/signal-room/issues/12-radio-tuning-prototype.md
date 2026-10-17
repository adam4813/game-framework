# Radio presets & spectrum read

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:prototype
Type: prototype
Status: open
Blocked by: 01, 04

## Question

Manual vernier tuning was dropped (see [Routine Cable route & station verbs](01-routine-cable-route.md)). The Radio now
jumps between 5 Presets (keys 1–5), each on a labelled spike in a spectrum, some of them decoys, and then Holds to copy
(~2 s). Tickets show a frequency label. Build a throwaway prototype of the spectrum display + preset jump + copy, and
judge: does matching a ticket's frequency label to a spike read at a glance under pressure, how should the spikes and
labels be drawn, and how should the procedural heterodyne tone carry the jump (static sweep → clean tone on a live
spike, dead air on an empty one) and the copy progress? Pick the label format and the frequency range/spacing that
reads best, and the copy duration.

Context: [Procedural audio on Raylib (desktop + WASM)](04-procedural-audio-research.md) is resolved. Drive the
heterodyne pitch through an atomic target smoothed in a callback synth. Expect ~43 ms latency on web, so judge the
feel on desktop first and check that it still reads on web.

Context from [Tickets, window & scoring for Shifts 1–2](05-tickets-window-scoring.md): a frequency only carries traffic
while an On Air Ticket names it; Stand-by frequencies are silent; decoy spikes are always on air. Shift 2 has 3 live
frequencies + 2 decoys across the 5 Presets. Judge how silent / live / decoy spikes read.
