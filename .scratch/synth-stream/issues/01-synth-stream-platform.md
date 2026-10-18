# Synth stream behind Platform

Status: ready-for-human
Source: raised during [Spec chunking & hand-off order](../../signal-room/issues/13-spec-chunking-order.md),
2026-10-17. Findings: `.scratch/signal-room/research/procedural-audio.md` on branch `research/procedural-audio`.

## Context

The engine's audio is file-based only (`Platform` does LoadSound/PlaySound). Signal Room's slice synthesises all its
audio. This is **framework work**: land it on its own branch off `main`, then rebase `feat/signal-room` onto it. It
must land before any slice implementation starts.

## Acceptance criteria

- [ ] `Platform::StartSynth(SynthSource*)` / `StopSynth()` over one mono f32 48 kHz stream using
      `SetAudioStreamCallback` (direct pull, through a static trampoline to the active source).
- [ ] The callback never locks, allocates, logs, or calls raylib or Flecs. Game→audio params go through
      `std::atomic<float>` targets smoothed in the callback; one-shots go through atomic trigger counters or one SPSC
      ring.
- [ ] Raylib-free DSP building blocks in `engine::audio` (sine, xorshift noise, biquad, envelopes), unit-tested.
- [ ] Web: audio resumes on the first click (or a `--pre-js` shim resumes it on keydown). The ScriptProcessorNode path
      is accepted, and AudioWorklet stays deferred.
- [ ] A Tracy zone around the callback, and a sandbox demo voice to verify on desktop and WASM.
