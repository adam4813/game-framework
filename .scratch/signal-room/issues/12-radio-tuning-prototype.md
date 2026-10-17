# Radio tuning feel

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:prototype
Type: prototype
Status: open
Blocked by: 01, 04

## Question

What should tuning the radio feel like? Build a throwaway prototype of the dial (coarse/fine or single vernier, 120–180 kHz), the lock zone width, what Drift does to a locked signal, and the procedural heterodyne tone as the main feedback (pitch falling to a clean tone at lock), plus the scope/needle visual. React to it and pick dial speed, lock tolerance, and how much the audio versus visual feedback carries.

Context: [Procedural audio on Raylib (desktop + WASM)](04-procedural-audio-research.md) is resolved. Drive the
heterodyne pitch through an atomic target smoothed in a callback synth. Expect ~43 ms latency on web, so judge the
feel on desktop first and check that it still reads on web.
