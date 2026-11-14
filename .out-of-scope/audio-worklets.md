# AudioWorklets for web audio

On the web, the synth stream runs on miniaudio's ScriptProcessorNode path, the one raylib's own `raudio.c` compiles
to under Emscripten. The engine does not move it to AudioWorklets.

## Why this is out of scope

ScriptProcessorNode renders audio in blocks of about 43 ms on the main thread, so a long frame can drop audio. The
synth stream (#6, "Synth stream behind Platform") accepted that trade-off. AudioWorklets would move rendering to the
audio thread, but every route there is costly for a prototype engine whose web build is a GitHub Pages demo:

- **Two copies of miniaudio, or a patched raylib.** Either the synth stream gets its own miniaudio device (vcpkg
  `miniaudio`, built with `MA_ENABLE_AUDIO_WORKLETS`) next to the copy compiled into raylib, which needs care to
  avoid duplicate symbols, or an overlay raylib port defines `MA_ENABLE_AUDIO_WORKLETS` in `raudio.c`.
- **Heavier link flags.** `-sAUDIO_WORKLET -sWASM_WORKERS -sASYNCIFY`, for every Game Target's web build.
- **Cross-origin isolation.** Worklets with WASM workers need COOP/COEP headers, which GitHub Pages can't set, so the
  page needs a `coi-serviceworker` shim or another host. itch.io embeds would need the same check.

The full findings are in `docs/research/procedural-audio.md`, section 4.2.

## When to revisit

Reopen this when a playtest hears dropouts or latency in a web build's synth audio that are traced to main-thread
frame time, not to the audio unlock or the buffer size. Start from the two options above.

## Prior requests

- #39: "Move the web synth stream to AudioWorklets"
