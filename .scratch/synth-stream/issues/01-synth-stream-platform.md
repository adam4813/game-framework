# Synth stream behind Platform

Status: ready-for-human
Source: raised during [Spec chunking & hand-off order](../../signal-room/issues/13-spec-chunking-order.md),
2026-10-17. Findings: `.scratch/signal-room/research/procedural-audio.md` on branch `research/procedural-audio`.

## Context

The engine's audio is file-based only (`Platform` does LoadSound/PlaySound). Signal Room's slice synthesises all its
audio. This is **framework work**: land it on its own branch off `feat/signal-room` and PR it back into `feat/signal-room` (the project's base branch;
it never merges into `main`). It
must land before any slice implementation starts.

## Acceptance criteria

- [x] `Platform::StartSynth(SynthSource*)` / `StopSynth()` over one mono f32 48 kHz stream using
      `SetAudioStreamCallback` (direct pull, through a static trampoline to the active source).
- [x] The callback never locks, allocates, logs, or calls raylib or Flecs. Game→audio params go through
      `std::atomic<float>` targets smoothed in the callback; one-shots go through atomic trigger counters or one SPSC
      ring.
- [x] Raylib-free DSP building blocks in `engine::audio` (sine, xorshift noise, biquad, envelopes), unit-tested.
- [x] Web: audio resumes on the first click (or a `--pre-js` shim resumes it on keydown). The ScriptProcessorNode path
      is accepted, and AudioWorklet stays deferred.
- [ ] A Tracy zone around the callback, and a sandbox demo voice to verify on desktop and WASM.

## Comments

**2026-10-17 — implemented; needs a human listen on web and a Tracy capture.** Framework work is on
`feat/imgui-debug-ui` (the framework branch, PR into `main`), not a branch off `feat/signal-room` as above:
the framework/spec git layout supersedes that line. `feat/signal-room` was restacked on top.

- Shipped:
  - `Platform::StartSynth(audio::SynthSource*)` / `StopSynth()`: one mono f32 48 kHz raylib
    `AudioStream` in pull mode, reached through a static trampoline in `RaylibBackend`. `NullPlatform`
    has no-op versions.
  - `engine::audio`: `SynthSource`, `SynthParam` (atomic float target), and `SynthTrigger` (atomic
    counter; fires between renders coalesce, so a backlog doesn't burst when web audio unlocks).
  - `audio/dsp/`: `SineOsc`, `WhiteNoise` (xorshift32), `Biquad` (RBJ LP/HP/BP), `AdEnvelope` and the
    `OnePole` smoother. Unit tests are `synth_dsp_test` and `synth_trigger_test`.
  - `profiling::StaticZone`: the existing `Zone` allocates a srcloc per call, so this new zone is
    allocation-free. It wraps the callback as `Platform::SynthCallback`.
  - `tools/wasm_audio_unlock.js` (`--pre-js`), and a sandbox "Synth Demo" scene (whistle/hiss sliders,
    click on button or Space).
  - Usage and the rules for `Render()` are in `src/engine/audio/README.md`.
- Decisions:
  - Noise is named `WhiteNoise`, not `XorshiftNoise`, so it isn't mistaken for `engine::noise`'s coherent
    procgen samplers.
  - DSP stays hand-rolled rather than using vcpkg miniaudio. raylib already compiles miniaudio in, with
    `MA_NO_GENERATION`, so a second implementation would clash. vcpkg miniaudio is the route for the
    AudioWorklet upgrade (see 02).
  - The shim wraps the `AudioContext` constructor rather than poking miniaudio internals, which closure
    may rename.
- Verified:
  - Desktop debug and release builds are clean; 17/17 ctest pass.
  - Sandbox runtime: switching in and out of the Synth scene through the Flecs remote API logs `STREAM:
    Initialized successfully (48000 Hz, 32 bit, Mono)` and a clean unload each time. The WASAPI device is
    48 kHz (so there's no resample) with a 1440-frame (30 ms) period, which answers an open question in
    the research doc.
  - An `ENABLE_TRACY=ON` build compiles, and the scene runs without crashing.
  - The WASM release build links, and the closure output keeps the shim intact. miniaudio constructs
    `window.AudioContext` at device init, so the shim sees it.
- Still needs a human:
  1. ~~Listen to the demo on desktop and in the browser.~~ Done: the user heard it on desktop and WASM.
  2. Confirm the `Platform::SynthCallback` zone in a Tracy capture (`desktop-profile` preset). There's no
     `tracy-capture` CLI to do this headlessly.
- Fixed along the way: CMake de-duplicates repeated bare `--pre-js` link options, so both now use `SHELL:`.

**2026-10-17 — PR open.** The framework branch `feat/imgui-debug-ui` (with the version 0.5.0 bump) is up as
https://github.com/adam4813/game-framework/pull/6 into `main`. Only the Tracy zone check is still open.
