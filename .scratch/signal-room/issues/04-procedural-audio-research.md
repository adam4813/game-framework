# Procedural audio on Raylib (desktop + WASM)

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:research
Type: research
Status: resolved
Blocked by: 

## Question

How can Signal Room synthesize audio at runtime through the `Platform` abstraction? Investigate Raylib `AudioStream` / `SetAudioStreamCallback` / `UpdateAudioStream` (the vcpkg-pinned version), buffer sizes and latency, threading constraints (the callback runs on the audio thread, so how does ECS state get across safely?), Emscripten/WASM behaviour (AudioWorklet vs ScriptProcessor, user-gesture unlock), and CPU cost of a few simultaneous voices (heterodyne whistle, relay clicks, noise-based hiss). Also note any small permissively licensed C/C++ DSP libs worth considering versus hand-rolled oscillators. Capture options and trade-offs.

## Answer

Findings: `.scratch/signal-room/research/procedural-audio.md` on branch `research/procedural-audio` (pushed, never
merged).

- raylib 5.5 + miniaudio 0.11.22 (vcpkg). The engine has no streaming yet (`Platform` only does LoadSound/PlaySound).
- Use **`SetAudioStreamCallback`** (direct pull), not `UpdateAudioStream`. The double buffer adds ≥ 33 ms and
  underruns on long frames. `SetAudioStreamBufferSizeDefault` doesn't shrink the device buffer.
- Callback constraints: no user-data pointer (needs a static trampoline to a global synth); variable chunk sizes
  (≤ 512 stereo / 1024 mono frames); runs **under raylib's global audio lock**. Never call raylib audio from inside it
  (deadlock on Windows), and keep it fast or `PlaySound` stalls.
- Threading: Flecs runs 4 workers. Continuous params go through `std::atomic<float>` targets smoothed in the callback;
  one-shots go through atomic trigger counters or one SPSC ring. Inside the callback: no locks, allocation, logging,
  raylib or Flecs calls.
- Latency: desktop ~10–30 ms. Web uses ScriptProcessorNode (2048-frame blocks, ~43 ms, main thread, so frame
  hitches cause dropouts). AudioWorklet needs a patched raylib, `-sAUDIO_WORKLET -sWASM_WORKERS -sASYNCIFY` and
  COOP/COEP headers (not on GitHub Pages). **Defer unless playtests show dropouts.**
- Web unlock: miniaudio resumes only on click/touchend, not keydown. The title screen needs a click, or a `--pre-js`
  shim that resumes on keydown.
- Cost: ~5 Mflop/s for 4 voices (an estimate, not measured; add a Tracy zone to confirm). **Hand-roll** sine, xorshift
  noise, biquad and envelopes (~200–300 LOC). `stb_hexwave` is available for richer timbres. No heavier library is
  needed.
- Proposed shape: `Platform::StartSynth(SynthSource*)` / `StopSynth()` over one mono f32 48 kHz callback stream in
  the raylib backend. DSP lives in `engine::audio` (Raylib-free, unit-testable), with voice params in JSON.
