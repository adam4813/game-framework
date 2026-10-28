# Move the web synth stream to AudioWorklets

Status: needs-triage
Blocked by: 01
Source: deferred by [Synth stream behind Platform](01-synth-stream-platform.md), 2026-10-17. Findings:
`docs/research/procedural-audio.md` §4.2.

## Context

On web the synth stream uses miniaudio's ScriptProcessorNode path: blocks of about 43 ms, run on the main
thread, so a long frame drops audio. Ticket 01 accepted that. Pick this up only if playtests show audible
web dropouts or latency.

## Options

- **Our own miniaudio device (vcpkg `miniaudio`) for the synth stream only.** Compile with
  `MA_ENABLE_AUDIO_WORKLETS` and keep raylib for file-based sounds. This needs no patched raylib port, but
  takes care to avoid duplicate symbols with the miniaudio compiled into raylib.
- **An overlay raylib port** that defines `MA_ENABLE_AUDIO_WORKLETS` in `raudio.c`.

Either way it needs `-sAUDIO_WORKLET -sWASM_WORKERS -sASYNCIFY`, and cross-origin isolation (COOP/COEP).
GitHub Pages can't set those headers, so that means a `coi-serviceworker` shim or another host.
