# Radio presets & spectrum read

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:prototype
Type: prototype
Status: resolved
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

## Answer

**Verdict (2026-10-17).** The prototype tried four displays: a panadapter, a glass dial, a preset bank and a waterfall.
The Panadapter read best. Playing it showed the bigger problem: a spectrum panel with presets plays like a console
minigame (closer to Among Us than Overcooked). So the spectrum is gone. The Radio becomes **machines in the room**,
and matching a Ticket to a Radio is done by walking to it, not by reading a panel.

**Radios are fixed-frequency Stations.** Each Radio is a Station tuned to one frequency. It shows that frequency on its
own display and has a **traffic lamp**. Shift 1 has 1 Radio. Shift 2 has 3, on 3 frequencies on an even half-MHz grid
(e.g. 7.0 · 7.5 · 8.0). Frequencies are Shift/Layout data. There are no Presets, no spectrum and no jump. The verb is
unchanged: Hold to copy, **2.0 s**. Leaving pauses the copy and keeps progress.

**Antenna Coupler: one Radio copies at a time, and the Operator chooses which.** There is no parallel copying. In the
lore, the room has one roof antenna, fed to one receiver at a time through a relay coupler.
- **Online:** a Radio's lamp is lit and its tone plays while an On Air Ticket names its frequency (the plain on-air
  rule from [Tickets, window & scoring](05-tickets-window-scoring.md)). With no Radio engaged, every Radio with traffic
  is lit, and any Operator can engage any of them.
- **Engage:** when an Operator starts Holding a lit Radio, the coupler switches the antenna to it. Every other Radio
  goes dark with dead-air hiss. This applies to the AI too: an AI copying makes the others go dark for the human.
- **Disconnect:** the coupler lets go when the copy completes, when the Operator releases the Hold or walks off (the
  Radio keeps its progress, as before), or when the frequency stops being On Air. A copy already in progress finishes,
  and its card may be stale.
- **Coupler reset:** after a disconnect, every Radio stays dark for `coupler_reset` seconds while the relay re-seats (a
  tuning knob, start at 0.4 s, keep it under 0.5 s). Then every Radio with traffic comes back online. Nothing is
  chosen at random.
- In Shift 1, with one Radio, this is just the traffic lamp plus a short reset blink after each copy.

**Decoys are dropped** from the slice. They may come back later as a Shift gimmick (backlog). If they return, the tell
that was chosen is a garbled tone plus a flickering display.

**Labels.** The Radio display and the Ticket show the same one-decimal MHz label on a half-MHz grid ("7.5"). Finer
fractions ("7.215") were too confusing under pressure.

**Audio** (callback synth; see [Procedural audio on Raylib](04-procedural-audio-research.md)):
- Each Radio has its own heterodyne pitch, low → high by frequency. Keep this: the pitch tells you which Radio is live.
- The coupled Radio plays a clean tone. While someone Holds to copy, the tone keys Morse, and that is the copy
  progress cue. Pitch rise and tick-rate cues were tried and not picked.
- Uncoupled Radios play dead-air hiss.
- Engaging a Radio: a relay clunk as the others cut to hiss. Disconnecting: a clunk and a short static sweep (~150 ms
  felt right in the prototype), then each Radio that comes back online plays the pull-in whistle into its tone.
- Web latency (+43 ms) was not re-judged for the receiver model.

**Prototype (primary source):** branch `prototype/radio-spectrum` (local, not merged; restacked with `feat/signal-room`), file
`.scratch/signal-room/prototypes/radio-spectrum.prototype.html`. It still shows the old preset/spectrum model.

Ripples: comments added to [Routine Cable route & station verbs](01-routine-cable-route.md),
[Tickets, window & scoring](05-tickets-window-scoring.md), [Room layout, movement & set-down](07-room-layout-movement.md),
[AI Operator behaviour](08-ai-operator-behaviour.md) and [Requisitions](10-requisitions.md). Glossary: removed
**Preset** and **Decoy**, added **Antenna Coupler** and **Coupler reset**. Decoys are
backlogged as [Decoy Radio as a Shift gimmick](../../backlog/issues/04-decoy-radio-gimmick.md).

## Comments

- 2026-10-17 (revised the same day): the first version had the coupler light **one random Radio** after each reset,
  filtered by unmet demand. That took away the player's choice. Now every Radio with traffic is online; engaging one
  takes the antenna and darkens the rest, and finishing (or letting go) brings them back after `coupler_reset`. The
  unmet-demand filter is dropped, since it only existed to stop the random pick getting stuck.
