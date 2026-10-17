# Research: Procedural audio on Raylib (desktop + WASM)

**Question.** How can Signal Room synthesize audio at runtime (heterodyne whistle tracking a tuning dial, relay
clicks, steam hiss, stamp thud) through the engine's `Platform` abstraction, on both desktop and WASM?

**Date:** 2026-10-17. **Pinned versions:** raylib **5.5** (vcpkg port `raylib` 5.5 port-version 1, feature `audio`),
with miniaudio **0.11.22** de-vendored from the vcpkg `miniaudio` port, under `builtin-baseline`
`dd3097e305afa53f7b4312371f62058d2e665320`. Both versions confirmed from the installed `vcpkg.spdx.json` files in
`build/desktop` and `build/wasm`, and from `RAYLIB_VERSION "5.5"` / `MA_VERSION_REVISION 22` in the installed headers.

Source keys used below:

- **[raudio]** raylib 5.5 `src/raudio.c` (from the vcpkg download `raysan5-raylib-5.5.tar.gz`)
- **[raylib.h]** installed `include/raylib.h` (5.5)
- **[ma]** installed `include/miniaudio.h` (0.11.22)
- **[port]** `vcpkg/ports/raylib/portfile.cmake` and `vcpkg.json`
- **[em-aw]** https://emscripten.org/docs/api_reference/wasm_audio_worklets.html
- **[em-ww]** https://emscripten.org/docs/api_reference/wasm_workers.html
- **[mdn-spn]** https://developer.mozilla.org/en-US/docs/Web/API/ScriptProcessorNode
- **[mdn-aw]** https://developer.mozilla.org/en-US/docs/Web/API/AudioWorklet
- **[mdn-sab]** https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer
- **[mdn-autoplay]** https://developer.mozilla.org/en-US/docs/Web/Media/Guides/Autoplay
- Local engine code: `src/engine/platform/platform.hpp`, `src/engine/platform/raylib/raylib_backend.cpp`,
  `src/engine/audio/*`, `CMakeLists.txt` (WASM link flags), `src/main.cpp`, `.github/workflows/deploy-wasm.yml`

---

## 1. Where the engine is today

- `Platform` exposes only `LoadSound` / `UnloadSound` / `PlaySound(int handle)` (`platform.hpp`, "── Audio ──").
  There is no stream, volume, pitch, or synthesis API.
- `RaylibBackend::Init` calls `InitAudioDevice()` and keeps a `std::vector<Sound>` plus a path-to-handle map
  (`raylib_backend.cpp`). Nothing calls `SetAudioStreamBufferSizeDefault` or any `AudioStream` function.
- `engine::audio` has `SoundEffect` (one-shot, fired by the `SoundEffectPlayback` system) and `Music`. `Music` is
  loaded as `AssetType::Sound`, and its comment says "Playback wiring beyond one-shot sounds is future work". So no
  streaming path exists yet, file-based or procedural.
- Desktop runs Flecs with `set_threads(4)` (`main.cpp`), so ECS systems may run on worker threads. Any game-to-audio
  parameter handoff must be thread-safe from several writers' threads, not only the main thread.
- WASM: `emscripten_set_main_loop(tickWrapper, 0, 1)`, with no `-pthread`, `-sWASM_WORKERS`, `-sAUDIO_WORKLET` or
  `-sASYNCIFY` in the link options (`CMakeLists.txt`). The build deploys to **GitHub Pages**
  (`deploy-wasm.yml`), which cannot set custom HTTP response headers.

## 2. raylib 5.5 `AudioStream` API: what it actually does

Public API ([raylib.h] lines ~1624–1702):

```c
typedef void (*AudioCallback)(void *bufferData, unsigned int frames);
AudioStream LoadAudioStream(unsigned int sampleRate, unsigned int sampleSize, unsigned int channels);
void UpdateAudioStream(AudioStream stream, const void *data, int frameCount);
bool IsAudioStreamProcessed(AudioStream stream);
void SetAudioStreamBufferSizeDefault(int size);
void SetAudioStreamCallback(AudioStream stream, AudioCallback callback);
void AttachAudioStreamProcessor(AudioStream stream, AudioCallback processor);
void AttachAudioMixedProcessor(AudioCallback processor);
// + Play/Pause/Resume/Stop/IsPlaying, SetAudioStreamVolume/Pitch/Pan
```

### 2.1 Two ways to feed a stream

| Mode | How it works ([raudio]) | Latency | Thread |
|---|---|---|---|
| **Push**: `UpdateAudioStream` | Stream is a double buffer of `2 × subBufferSize` frames. The game polls `IsAudioStreamProcessed`, then fills one half under `AUDIO.System.lock` (`LoadAudioStream`, `UpdateAudioStream`, `ReadAudioBufferFramesInInternalFormat`). | At least one sub-buffer queued ahead, plus the device period. The default sub-buffer is `sampleRate/30` frames (about 33 ms), raised to at least the device period. Total is roughly 33–67 ms plus the device buffer. Frame-rate dependent: a long frame causes underruns. | Game thread writes. Audio thread reads. |
| **Pull**: `SetAudioStreamCallback` | When `buffer->callback` is set, `ReadAudioBufferFramesInInternalFormat` calls `callback(framesOut, frameCount)` directly and skips the double buffer entirely ([raudio] ~L2346–2353). | Only the device period plus the format/resample step. | **The audio thread** (desktop). On web the thread depends on the backend (see section 4). |

Key details of the pull mode, read from the source:

- **No user-data pointer.** `AudioCallback` is `void(*)(void*, unsigned)`. The `void*` is the output buffer, not a
  context pointer. The callback must therefore be a free/static function reaching a global or static synth instance.
  This rules out more than one independent "instance" per function unless you write a trampoline per stream.
- **Callback chunk size is small and variable.** `ReadAudioBufferFramesInMixingFormat` pulls input through a fixed
  `ma_uint8 inputBuffer[4096]`, so one callback asks for at most 4096 / bytes-per-frame frames. That is 512 frames for
  stereo f32 and 1024 for mono f32. With resampling it may be called with odd counts. The outer mixer also uses a
  `float tempBuffer[1024]` (512 stereo frames) per step ([raudio] ~L2445, ~L2514). DSP code must handle any `frames`
  value, including counts that are not a multiple of a block size.
- **The callback runs while raylib's global audio mutex is held.** `OnSendAudioDataToDevice` takes
  `AUDIO.System.lock` around the whole mix loop, including stream callbacks and processors ([raudio] ~L2496–2580).
  Consequences:
  1. **Never call any raylib audio function from inside the callback.** `PlaySound`, `SetAudioStreamVolume`,
     `UpdateAudioStream`, and the rest all lock `AUDIO.System.lock`. On Win32 `ma_mutex` is an auto-reset
     `CreateEventA` ([ma] `ma_mutex_init__win32`), so it is not recursive and would self-deadlock.
  2. Every main-thread raylib audio call (`PlaySound` from `SoundEffectPlayback`, for example) waits on the same lock.
     A slow callback therefore stalls the game thread as well. Keep the callback cheap and bounded.
- **Pitch/volume/pan still work on callback streams.** Volume and pan are applied in `MixAudioFrames`. Pitch changes
  the `ma_data_converter` output rate (`allowDynamicSampleRate = true` in `LoadAudioBuffer`). For a synth, it is
  better to control frequency inside the oscillator and leave raylib pitch at 1.0.
- **Format/resampling.** The device mixes in f32 stereo at the device's native rate (`AUDIO_DEVICE_FORMAT
  ma_format_f32`, `AUDIO_DEVICE_CHANNELS 2`, `AUDIO_DEVICE_SAMPLE_RATE 0` = device default, [raudio] / `config.h`).
  Each stream has its own `ma_data_converter`. If `LoadAudioStream`'s rate differs from the device rate, the stream is
  resampled (linear resampler, cheap). raylib 5.5 does not publicly expose the device sample rate. Pick 48000 Hz,
  which matches most desktop devices and most browser `AudioContext`s, or accept the small resample cost.
  `sampleSize = 32` means f32 input, so no integer conversion is needed.

### 2.2 `SetAudioStreamBufferSizeDefault`

This sets `AUDIO.Buffer.defaultSize`, which **only affects streams created afterwards** and only the push-mode double
buffer size (`subBufferSize`, clamped up to the device period) ([raudio] `LoadAudioStream`). It does **not** change
the device period. In pull mode the double buffer is allocated but bypassed, so the setting barely matters. It cannot
lower device latency, because raylib passes no `periodSizeInFrames` / `periodSizeInMilliseconds` to
`ma_device_config` (`InitAudioDevice`).

### 2.3 Device latency raylib actually gets

- **Desktop.** `ma_device_config_init` defaults to `ma_performance_profile_low_latency` (enum value 0, [ma] ~L4338).
  The default period is `MA_DEFAULT_PERIOD_SIZE_IN_MILLISECONDS_LOW_LATENCY = 10` ms with `MA_DEFAULT_PERIODS = 3`
  ([ma] ~L12166–12171). The backend may round this (WASAPI shared mode is usually about 10 ms). raylib logs the
  real value at startup: `AUDIO: ... Periods size: N`. So the dial-to-whistle response is about 10–30 ms on desktop,
  which is fine for a tuning dial.
- **Web (ScriptProcessor path, the current build).** `ma_calculate_period_size_in_frames_from_descriptor__webaudio`
  uses 33 ms for low-latency and then rounds **up to the next power of two** within 256..16384 ([ma] ~L40201–40236).
  At 48 kHz, 1584 frames becomes **2048 frames, about 43 ms** per `onaudioprocess` block. At 44.1 kHz, 1455 frames
  also becomes 2048, about 46 ms. Expect roughly 45–90 ms end to end. That is noticeable but tolerable for a dial
  whistle. Relay clicks will feel slightly late.

## 3. Threading: getting ECS state to the callback safely

The facts from section 2: the callback runs on miniaudio's device thread on desktop, holds raylib's global lock, and
Flecs systems run on up to 4 worker threads. Recommended pattern (standard real-time audio practice; consistent
with the Emscripten worklet guidance "should execute as quickly as possible and be non-blocking" and its suggestion
to share data via lock-free atomics [em-aw]):

1. **Continuous parameters** (whistle target frequency, hiss level, master gain): one `std::atomic<float>` each,
   written by the game with `memory_order_relaxed` and read once per callback. Check
   `std::atomic<float>::is_always_lock_free`, which holds on x64 and wasm32. **Smooth inside the callback**, using a
   one-pole lerp toward the target per sample or a linear ramp per block. Without smoothing, a 60 Hz parameter
   update produces zipper noise, which is very audible on a pure sine whistle.
2. **One-shot triggers** (relay click, stamp thud): either
   - a monotonically increasing `std::atomic<uint32_t>` counter per voice. The callback compares it with its last
     seen value and starts that many envelopes. This is simplest and cannot lose events, but carries no
     per-event data; or
   - a fixed-capacity **SPSC ring buffer** of small POD events (`{voiceId, gain, pitch}`) when per-event parameters
     are needed. Because Flecs systems may run on several threads, either make the producer side MPSC or post
     events from a single-threaded system phase (or a `.multi_threaded(false)` system) so only one producer exists.
3. **Inside the callback, avoid** heap allocation, `std::mutex`, logging (spdlog), raylib calls, Flecs world access,
   and `std::function`. Preallocate voice state at init.
4. **Ownership.** Because `AudioCallback` has no user pointer, the backend owns one static `Synth* g_synth`. Set it
   before `SetAudioStreamCallback` and clear it only after `StopAudioStream` + `UnloadAudioStream`. Unload takes the
   audio lock, so the callback will not be mid-flight once it returns.

On **web without threads** (the current build), the ScriptProcessor callback runs on the main JS thread between
frames, so there is no data race at all. The atomics still compile and cost nothing, and the same code then works
unchanged if the build later moves to AudioWorklets.

## 4. Emscripten / WASM behaviour

### 4.1 What the current build gets

- The vcpkg port builds raylib with `-DPLATFORM=Web` and does not define `MA_ENABLE_AUDIO_WORKLETS` ([port];
  `raudio.c` defines only `MA_NO_*` decoder flags before including miniaudio). miniaudio 0.11.x therefore uses
  **ScriptProcessorNode**. The [ma] Web Audio section says: "TODO: Version 0.12: Swap this logic around so that
  AudioWorklets are used by default."
- `ScriptProcessorNode` is **deprecated** ("replaced by AudioWorklets"). Its buffer must be a power of two between
  256 and 16384 [mdn-spn]. Its `onaudioprocess` runs on the **main thread**. This is the well-known reason it
  glitches under load. MDN's page does not say so outright, but miniaudio's JS calls
  `_ma_device_process_pcm_frames_playback__webaudio` from that handler on the same thread as
  `emscripten_set_main_loop`. **Practical effect:** a long game frame such as a level load or a GC pause delays audio
  callbacks and produces dropouts. The 2048-frame (about 43 ms) block gives some cushion.
- **No `SharedArrayBuffer`, COOP/COEP, or threads are needed** for this path.

### 4.2 Upgrading to AudioWorklets (optional, later)

- miniaudio: define `MA_ENABLE_AUDIO_WORKLETS` and link with `-sAUDIO_WORKLET=1 -sWASM_WORKERS=1 -sASYNCIFY` ([ma]
  section 2.6). Emscripten's docs list `-sAUDIO_WORKLET -sWASM_WORKERS` [em-aw].
- Since miniaudio is compiled *inside* raylib's `raudio.c`, this needs an **overlay port or a patched raylib build**
  that adds the define to `raudio.c`'s compile flags. It is not a game-side CMake change.
- **Wasm Workers require `SharedArrayBuffer`**. The server must send the "needed COOP+COEP headers to enable
  SharedArrayBuffer capabilities" [em-ww], and the page must be a secure context and cross-origin isolated
  [mdn-sab]. AudioWorklet itself is secure-context only [mdn-aw]. **GitHub Pages cannot set COOP/COEP headers**, so
  this would need a service-worker shim (e.g. `coi-serviceworker`) or a different host. `-sASYNCIFY` also increases
  code size and needs care with `emscripten_set_main_loop`.
- Benefit: audio renders on the Web Audio thread [mdn-aw] in 128-frame render quanta (Web Audio spec), so it is
  immune to main-loop jank and has lower latency.
- **Verdict:** not worth it for the first pass. Revisit only if web dropouts are audible in playtests.

### 4.3 User-gesture unlock

- Browsers block Web Audio until user activation such as a click, tap, or key press [mdn-autoplay].
- miniaudio 0.11.22 handles this itself. It registers capture-phase `document` listeners for **only `touchend` and
  `click`**, calls `AudioContext.resume()`, then removes them ([ma] `miniaudio.unlock_event_types`, ~L40873–40898).
- **Gotcha:** `keydown` is **not** in miniaudio's list. If a player's first input is a key press, the context stays
  suspended until they click. Mitigations: make the title screen require a click ("Click to start"), which costs
  nothing, or add a small `--pre-js` listener for `keydown`/`pointerdown` that resumes all `AudioContext`s.
- While suspended, the callback simply is not called. The synth must therefore not assume time advances, and
  trigger counters or event rings should be bounded or coalesced so a burst does not play when audio unlocks.

## 5. CPU cost of the planned voices

Back-of-envelope figures: about 48,000 frames/s, mono synthesis duplicated to stereo by raylib's pan stage.

| Voice | Technique | Approximate ops/sample |
|---|---|---|
| Heterodyne whistle | Two sine oscillators (carrier + local oscillator) multiplied, or one sine at the difference frequency, plus a little noise or detune "warble"; one-pole smoothing on the frequency | about 20–40 flops (phase-accumulator + `sinf`, or a polynomial/wavetable sine for less) |
| Steam hiss | xorshift32 white noise, then a biquad band-pass or high-pass, then a slow random LFO on gain | about 15 |
| Relay click | Very short (1–5 ms) noise or impulse burst with exponential decay, optionally through a resonant biquad for "metal" | about 10, only while active |
| Stamp thud | Sine at 50–120 Hz with a fast pitch drop (exponential envelope), plus a low-passed noise transient | about 20, only while active |

Worst case is roughly 100 flops × 48 k ≈ **5 Mflop/s**, which is well under 0.1% of one modern desktop core. Scalar
WASM is typically within about 1.5–2× of native, so still negligible. The real costs on this stack are elsewhere:
raylib's mutex and mixer copy per stream (about one 512-frame pass each), and on web the main-thread scheduling of
ScriptProcessor callbacks. **Mixing all procedural voices into one `AudioStream`** keeps that overhead to one stream.
These are estimates, not measurements. Confirm with a Tracy zone in the callback on desktop.

## 6. Libraries vs. hand-rolled

| Option | License | vcpkg | Fit |
|---|---|---|---|
| **Hand-rolled** oscillators, noise, biquads, and AD/ADSR envelopes (about 200–300 lines of C++) | n/a | n/a | Covers all four voices. The whistle is a sine (aliasing is a non-issue for sines). Hiss and thud are noise plus filters. No dependency, unit-testable, and easy to drive from JSON data. **Recommended.** |
| **stb_hexwave** (`stb_hexwave.h`) | Public domain / MIT | **Already installed** via the `stb` port (present in `vcpkg_installed/.../include`) | Band-limited, morphable oscillator (saw/square/triangle family). Useful if a buzzier, harmonically rich "radio" tone is wanted later. Zero new dependency. |
| **miniaudio `ma_waveform` / `ma_noise` / `ma_lpf` / `ma_biquad`** | Unlicense / MIT-0 | `miniaudio` port present | raylib compiles miniaudio with **`MA_NO_GENERATION`** ([raudio] ~L173), so `ma_waveform`/`ma_noise` are not in the linked raylib object. A second `MINIAUDIO_IMPLEMENTATION` TU would duplicate symbols already in raylib. Filters (`ma_biquad`, `ma_lpf`) are compiled in but are not exported through raylib's API. **Not practical** without patching. |
| **DaisySP** | MIT (with a separate LGPL submodule, which should be avoided) | No port | Rich (oscillators, filters, drums, physical models). Requires a build (Makefile/CMake). Overkill for four voices. A good source to *copy* individual algorithms from (e.g. `AnalogBassDrum` for the thud). |
| **SoLoud** | zlib/libpng | No port | A whole audio engine (would duplicate or replace raylib audio) with sfxr-style generation. Too heavy and overlaps raylib. |
| **Maximilian, STK** | MIT / MIT-like | No ports | Usable but more than needed. STK is large. |
| **Faust** (generate C++ from DSP code) | GPL compiler, but generated code may be used under any license | No port | Powerful offline codegen. Adds a toolchain step. Not justified now. |

(DaisySP and SoLoud licenses from their GitHub READMEs. vcpkg port presence checked against the local
`vcpkg/ports` tree, which may be newer than the baseline. `stb` and `miniaudio` are confirmed installed.)

## 7. Options for the Platform abstraction

**A. Pull-mode synth stream owned by the backend (recommended).**
Add to `Platform` something like:

```cpp
// ── Procedural audio ──
// Starts a single procedural output stream rendered by `synth` on the audio thread.
// `synth` must outlive StopSynth(); Render() is called with no locks safe to take and must not allocate.
virtual void StartSynth(SynthSource* synth) = 0;
virtual void StopSynth() = 0;
```

Here `SynthSource` is an engine-side (Raylib-free) interface with `virtual void Render(float* out, unsigned frames,
unsigned channels, unsigned sampleRate) noexcept`. `RaylibBackend` keeps a static `SynthSource*` and a static
trampoline passed to `SetAudioStreamCallback`. It creates the stream with `LoadAudioStream(48000, 32, 1)` (mono f32),
followed by `PlayAudioStream`. All DSP, voices, parameter atomics, and event rings live in `engine::audio`, which is
plain C++ and testable with gtest without a device. Game code only writes parameters and triggers through an ECS
singleton or component. This fits ADR 0001 and AGENTS.md (no Raylib in game logic; voice definitions such as envelope
times and filter frequencies can come from JSON).

**B. Push mode (`UpdateAudioStream` from an ECS system).** This is simpler on paper and single-threaded (no atomics),
but latency becomes frame-rate dependent (sub-buffer of at least 33 ms queued ahead), underruns occur on frame hitches,
and on web it adds to the already 43 ms ScriptProcessor block. **Not recommended** for an interactive dial.

**C. Pre-render to `Wave` → `LoadSoundFromWave` at load time** for one-shots (click, thud) with randomised variants,
plus option A only for the continuous whistle and hiss. This is a valid hybrid if one-shot timing must be
sample-accurate with existing `SoundEffect` plumbing. It reuses `PlaySound`, but one-shots then pass through
raylib's lock from the main thread, and live parameterisation is lost. A reasonable fallback, but A handles
everything in one place.

**D. `AttachAudioMixedProcessor`.** This runs on the final mix and also has no user pointer. It is meant for effects,
not generation. Not useful here except perhaps for a master "radio" filter later.

## 8. Recommendation

1. Implement **option A**: one mono f32 `AudioStream` at 48 kHz in **pull mode** (`SetAudioStreamCallback`), wrapped
   by a new `Platform::StartSynth/StopSynth` pair and a static trampoline in `RaylibBackend`. Do not use
   `UpdateAudioStream`, and do not rely on `SetAudioStreamBufferSizeDefault` (it does not affect pull-mode latency).
2. Put a **hand-rolled DSP kit** in `engine::audio`: phase-accumulator sine, xorshift noise, RBJ biquad, AD
   envelope, and one-pole smoother. Make voices data-driven from JSON. Keep `stb_hexwave` in reserve for richer
   timbres, since it is already available at zero cost.
3. **Handoff:** `std::atomic<float>` targets plus in-callback smoothing for continuous controls, and atomic trigger
   counters (or one SPSC ring fed from a single-threaded system) for one-shots. In the callback: no locks, allocation,
   logging, or raylib/Flecs calls.
4. **Web:** ship on the default ScriptProcessor path (no COOP/COEP; works on GitHub Pages). Expect about 45–90 ms
   latency and possible dropouts on long frames. Ensure the title screen's first interaction is a **click** (miniaudio
   unlocks only on `click`/`touchend`) or add a `keydown` resume shim in `--pre-js`. Revisit AudioWorklets (overlay
   raylib port + `-sAUDIO_WORKLET -sWASM_WORKERS -sASYNCIFY` + cross-origin isolation) only if playtests show
   problems.
5. **Verify at runtime:** read raylib's startup log for backend, rate, and period on both targets. Add a Tracy zone
   around `Render` on desktop to confirm callback cost is in the microsecond range.

## Open questions / not verified

- The actual WASAPI period on the dev machine (read from raylib's `Periods size` log line).
- Whether `ma_device_process_pcm_frames_playback__webaudio`'s 2048-frame block holds up under the game's worst
  frame times in the browser. This needs a playtest.
- vcpkg port presence for DaisySP/SoLoud/STK was checked against the local, possibly newer, ports tree rather than the
  exact baseline commit. All were absent there.
