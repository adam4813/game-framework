# Research: SDL3 and miniaudio behind Platform, on desktop and web

Ticket: "SDL3 and miniaudio on both targets" (render pipeline map, wayfinder research).
Question: do SDL3 (window, input, text input, gamepads) and miniaudio + stb_vorbis cover what `Platform` does today
through raylib on desktop and web, and what does GLFW offer instead?
Out of scope: the WebGPU renderer, Dawn's build cost, and the shape of the split interfaces.

Date: 2026-11-25. Every source below was opened on that date. Line numbers refer to the exact versions listed.

**Short answer.** Yes, with work the plan doesn't list. SDL3 and miniaudio cover every non-drawing virtual on desktop.
On web they cover all of it too, but five things need our own code: the real pointer-lock state, fullscreen denial, a
device-pixel-ratio change without a resize, frame-rate caps, and saves. IME composition on web is covered by **neither**
SDL3 nor GLFW. miniaudio keeps `SynthSource` and stays on ScriptProcessorNode, with no cross-origin isolation.

## Sources (primary)

| Ref | Source |
|-----|--------|
| [local] | This repo, `origin/main` at `2af9cdf`: `src/engine/platform/platform.hpp`, `raylib/raylib_backend.cpp`, `file_save_store.cpp`, `src/engine/audio/synth_source.hpp`, `cmake/GameTarget.cmake`, `tools/wasm_shell.html`, `tools/wasm_audio_unlock.js`, `.github/` |
| [vcpkg] | vcpkg at `builtin-baseline` `2f42d24513a1…`, `D:\src\vcpkg\ports/`: **sdl3 3.4.20**, **miniaudio 0.11.26**, **glfw3 3.5.1**, stb 2026-08-02, dawn 20261111.144539, imgui 1.93.0, sdl3-mixer 3.2.4 |
| [sdl] | SDL 3.4.20 source, the archive `ports/sdl3/portfile.cmake` pins (SHA-512 matched): `src/video/emscripten/`, `src/joystick/emscripten/`, `src/events/`, `docs/README-emscripten.md` |
| [sdl-3.2.4] | SDL 3.2.4 source, the archive `tools/ports/sdl3.py` in Emscripten 4.0.10 pins (SHA-512 matched) |
| [glfw] | GLFW 3.5.1 source, from the vcpkg download |
| [em] | Emscripten 4.0.10 (local emsdk, and `EM-VERSION` in all three workflows): `src/lib/libglfw.js`, `src/lib/libhtml5.js`, `tools/ports/sdl3.py`, `tools/ports/contrib/glfw3.py`, `ChangeLog.md` |
| [em-main] | Emscripten `main` on GitHub (7.0.2 in development): `ChangeLog.md`, `tools/ports/sdl3.py`, `tools/ports/contrib/glfw3.py`, `src/lib/libglfw.js`, `wasm_workers.rst`, `wasm_audio_worklets.rst` |
| [em-glfw] | pongasoft emscripten-glfw 3.4.0.20250305, the zip `contrib/glfw3.py` in 4.0.10 pins (SHA-512 matched) |
| [ma] | miniaudio 0.11.26 `miniaudio.h`, `CHANGES.md`, from the vcpkg download |
| [emdawn] | `emdawnwebgpu_pkg-v20261111.144539.zip`, the file `ports/dawn/portfile.cmake` pins (SHA-512 matched), `library_webgpu.js` |
| [raylib] | raylib 5.5 source, from the vcpkg download |
| [plock] | W3C Pointer Lock 2.0 editor's draft, https://w3c.github.io/pointerlock/ |
| [#72] | `gh issue view 72` (unscaled mouse delta), read only |
| [probe] | A throwaway page, see "What was run" |

### What was run [probe]

SDL 3.4.20 was built with Emscripten 4.0.10 using the portfile's options that apply there (no warnings), and linked
into one page with emdawnwebgpu, miniaudio 0.11.26 and stb_vorbis: `-O3 --closure 1`, `-sALLOW_MEMORY_GROWTH=1`, no
threads, no Asyncify. It ran in **headless Chrome 158 on Windows**, driven over CDP at a device pixel ratio of 2. The whole
page is 1,146,203 bytes of wasm and 92,369 of JS; that is an upper bound for all four together, not SDL's share.

Headless Chrome refused pointer lock (`WrongDocumentError`) and fullscreen (`not granted`). That exercised the denial
paths, and left the success paths **unconfirmed**. The vcpkg ports themselves were not built through vcpkg, and nothing was
run on desktop, in Firefox or in Safari.

## 1. SDL3 on Emscripten

### Which SDL3: the vcpkg port, not `--use-port=sdl3`

| | vcpkg `sdl3` | `--use-port=sdl3` |
|---|---|---|
| Version with our 4.0.10 | **3.4.20** [vcpkg] | **3.2.4** [em `sdl3.py`] |
| Newest | follows the baseline | 3.4.2 [em-main `sdl3.py`] |
| Status | a normal port, same source as desktop | warns `sdl3 port is still experimental` in 4.0.10; no longer considered experimental from 6.0.10 (2026-09-21) [em-main ChangeLog] |
| Window fills the page, gamepad d-pad as hat, rumble | yes | no: 3.2.4 has no `SDL_WINDOW_FILL_DOCUMENT`, sets `nhats = 0`, and its rumble entry points are stubs [sdl-3.2.4 `SDL_sysjoystick.c` L321, L391] |
| Threads | off unless the `emscripten-pthreads` feature is asked for [vcpkg] | follows `-pthread` |

Use the vcpkg port on both targets: one version everywhere, and the emsdk port is too old on our pinned emsdk. Never
link both. 3.4.20 **builds with 4.0.10** [probe], so SDL3 doesn't force an emsdk bump.

### Main loop

SDL doesn't own the loop. Its README shows the same `emscripten_set_main_loop(fn, 0, 1)` our `main.cpp`s use, or
optional main callbacks [sdl README-emscripten]. The only places SDL changes the loop's timing are its GL swap interval
and its own main-callback rate hint, which a WebGPU window with `main()` never touches [sdl `SDL_emscriptenopengles.c`
L47–66, `SDL_sysmain_callbacks.c`]. Asyncify isn't needed: `emscripten_sleep` is called only when the build has it.

`SDL_main` is not needed for anything we use. It only moves `SDL_*` URL parameters into hints and mounts IDBFS when
that was compiled in [sdl `SDL_sysmain_runapp.c`].

### Canvas size and device pixel ratio

- The default selector is `#canvas`, which is our shell's id [sdl `SDL_emscriptenvideo.c` L583–587].
- `SDL_WINDOW_HIGH_PIXEL_DENSITY` sizes the drawing buffer at CSS size × `devicePixelRatio`. Window coordinates stay
  CSS pixels; `SDL_GetWindowSizeInPixels` gives the buffer (L595–599, L670–678). Confirmed: a 1000×500 page at ratio 2
  gave `window 1000x500 pixels 2000x1000` [probe].
- **Two ways to fill the page.** `SDL_WINDOW_FILL_DOCUMENT` (new in 3.4.0) moves every other child of `<body>` into a
  hidden `<div>` (L484–508), so the shell's "Loading…" element, and any "no WebGPU" message put beside the canvas, would
  disappear. Instead give the canvas a **CSS size** in the shell and use `SDL_WINDOW_RESIZABLE`: SDL detects the CSS size
  and follows it on window resize (L601–610, `SDL_emscriptenevents.c` L531–563). Confirmed for load and resize [probe].
  Today's shell gives the canvas no size, because raylib sets it [local], so it has to change.
- **Gap: the ratio is re-read only on a window `resize` event.** Changing only the ratio (2 → 1) left a 1600×800 buffer
  on an 800×400 page [probe]. Whether a real move between monitors always fires `resize` is **unconfirmed**. Emscripten's
  bundled GLFW listens with `matchMedia` instead [em `libglfw.js` L1400–1402]; a `matchMedia` listener that dispatches a
  `resize` would cover SDL.

### A WebGPU-owned canvas: no conflict

- SDL creates a WebGL or 2D context only if asked (`SDL_GL_CreateContext`, the window framebuffer, `SDL_Renderer`). A
  plain `SDL_CreateWindow` touches none on Emscripten [sdl `SDL_video.c` L285–303]. Don't use `SDL_WINDOW_OPENGL` or
  `SDL_Renderer`.
- emdawnwebgpu makes its surface from a CSS selector with `getContext('webgpu')`, and `wgpuSurfaceConfigure` sets
  `canvas.width/height` itself [emdawn `library_webgpu.js` L2011–2046, L2588–2609]. SDL sets the same two values. They agree as long as the
  surface is configured with `SDL_GetWindowSizeInPixels` on every `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`.
- **Order matters.** `SDL_CreateWindow` sets the canvas to 1×1 to detect CSS sizing, and `SDL_DestroyWindow` sets it to
  0×0 (L601–603, L691–695). Create the window first, then configure the surface. A device acquired by the shell before
  `main` doesn't touch the canvas, so it fits the plan.
- Confirmed: SDL window, WebGPU clear every frame, resize and reconfigure on one canvas, with `--closure 1`, no
  validation errors [probe].

### Pointer lock and relative mouse (#72)

- **Delta units.** SDL reports `event.motion.xrel/yrel` in window units, never in our logical canvas, so it is the delta
  #72 asks for, beside today's canvas-scaled one. On desktop, relative mode is unscaled by default:
  `SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE` defaults to "0" [sdl `SDL_hints.h` L2918–2934, `SDL_mouse.c` L733–745].
- **On web it is the OS-accelerated `movementX`** × (window width / CSS width), which is 1 with the setup above
  [sdl `SDL_emscriptenevents.c` L711–734]. `requestPointerLock()` is called with no options [em `libhtml5.js`
  L1678–1690], so `unadjustedMovement` is off and the platform's acceleration applies [plock]. `movementX` is defined as
  a difference of `screenX` [plock]. Not affected by the window or canvas scale, so #72 holds; "raw" does not hold on web.
  `unadjustedMovement: true` needs our own `requestPointerLock` call; GLFW's ports don't offer it either (section 2).
- **Surprise 1: SDL can't tell you whether the lock is held.** `SDL_GetWindowRelativeMouseMode` returns the request. SDL
  tracks the real state privately (`has_pointer_lock`, L302–308) and reports no event. With the lock refused, and again
  after it was released, it stayed 1 while `emscripten_get_pointerlock_status` said 0 [probe]. `IsCursorLocked()` must
  ask `emscripten_get_pointerlock_status` on web, as raylib's callback does in effect today
  [raylib `rcore_web.c` L1687–1689].
- **Surprise 2: after Esc, mouse motion events stop** until the lock returns, because SDL drops absolute motion while the
  window is in relative mode (`SDL_mouse.c` L824–829). Menus would see a frozen cursor. So on a lost lock the backend must
  call `SDL_SetWindowRelativeMouseMode(win, false)`.
- **Surprise 3:** `SDL_GetRelativeMouseState` accumulates those dropped absolute moves as well (L789–790); the probe read
  back `-80,-20` with no motion event. Sum `event.motion.xrel` instead.
- SDL re-requests the lock on the next mouse press while relative mode is on (L685–688). `SDL_SetWindowRelativeMouseMode`
  outside an input handler is deferred to the next one (`deferUntilInEventHandler = 1`).
- **Linux, vcpkg only:** the portfile turns off `SDL_X11_XINPUT` [vcpkg], and then X11 relative mode returns
  `SDL_Unsupported` [sdl `SDL_x11mouse.c` L448–454, `sdlchecks.cmake` L418]. Cursor lock through this port should work on
  Wayland (not run), and **not on X11**, which is a step down from GLFW. It needs an overlay port or `VCPKG_CMAKE_CONFIGURE_OPTIONS`
  (the trick `triplets/wasm32-emscripten.cmake` already uses for raylib) if X11 matters. CI only needs headless.

### Text input, IME, key repeat

- Text events are **off by default** and must be started per window: "Text input events are not received by default", and
  starting them can activate an IME that swallows key presses [sdl `SDL_keyboard.h` L359–385]. raylib has no such switch, so
  `Input` needs a "text input wanted" call, driven by the UI's focused text field and by ImGui. Leaving it on all the
  time would let a desktop IME capture WASD.
- **Web has no IME.** SDL's Emscripten driver registers `keydown`, `keyup` and `keypress` only. Text comes from
  `keypress` when the key is one character [sdl `SDL_emscriptenevents.c` L465–476, L1365–1367]; there is no
  `composition*` listener, no on-screen keyboard and no text area (searched `src/video/emscripten`). `é` arrived [probe].
  CJK input and phone keyboards would need a hidden `<input>` of our own. Desktop has real IME (WM_IME, and the Linux
  port's default `ibus` feature).
- No regression: `TextInput()` today passes only ASCII 32–126, newline and tab [local `raylib_backend.cpp` L1544–1555].
- Key repeat: one `SDL_EVENT_KEY_DOWN` with `repeat` set. Confirmed on web [probe]. Covers `IsKeyPressedRepeat`.
- **Browser shortcuts are swallowed.** SDL calls `preventDefault` for every key it posts an event for. F5, F12, Ctrl+R and
  Tab were all prevented [probe]; with text input off, so are the others by source (L438–462). Emscripten's GLFW, used
  today, prevents only Backspace and Tab [em `libglfw.js` L417–426]. Devtools by F12 and reload by F5 stop working in our
  builds unless an event filter or `SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT` ("#canvas") limits it.
- Keyboard events bind to `window` by default, so the canvas needs no focus, and blur resets all keys (L349–364).
- Mouse wheel: floats, normalized per `deltaMode` (L323–347).

### Fullscreen

`SDL_SetWindowFullscreen` uses `emscripten_request_fullscreen_strategy` deferred to a user gesture, with a high-DPI
buffer when the window asked for one [sdl `SDL_emscriptenvideo.c` L710–767]. On desktop it is borderless fullscreen by
default, what `SetFullscreen` promises.

**Surprise: denial isn't handled.** SDL listens to `fullscreenchange` only, and clears `fullscreen_change_in_progress` there
alone (L478–494). When Chrome refused, SDL still sent `resized 800x600` and `enter fullscreen`, left the canvas at that
size, and every later call failed [probe]. An iframe without fullscreen permission would hit this. Guard it: check
`document.fullscreenEnabled` first and read `IsFullscreen()` from `emscripten_get_fullscreen_status`.

### Gamepads

- Hot-plug events on both targets. On web it comes from `gamepadconnected`/`gamepaddisconnected`, which still need the
  player's first press [sdl `SDL_sysjoystick.c` L447–493; `docs/research/gamepad-multi-input.md`].
- This fixes the raylib gaps listed in `gamepad-multi-input.md`: analog triggers on web (button values 6/7 become axes
  4/5, L279–326), Guide (`guide:b10` in the web default mapping, `SDL_gamepad_db.h` L874–876), no 4-pad limit, and unmapped
  desktop pads are told apart (`SDL_IsGamepad`).
- **Not fixed:** on web that default mapping applies to *every* pad, including ones the browser marks unmapped
  (`SDL_gamepad.c` L2388–2390, L3214). SDL's "standard" mark is only in the GUID subtype, so such a pad can read wrong.
- Rumble: desktop (XInput, HIDAPI, evdev, MFi), and web through `vibrationActuator.playEffect('dual-rumble')`, plus
  trigger rumble where the browser lists it (L591–607, L715–758). Support is asked for per pad. Not run on hardware.
- The Linux port has `SDL_LIBUDEV=OFF` [vcpkg]; SDL has an inotify fallback for hot-plug (`SDL_sysjoystick.c`). Not run.

### Rest of the window group

- `ShouldClose`: `SDL_EVENT_QUIT`. Never on web, as now; page close sends `SDL_EVENT_TERMINATING`, seen only by an event
  watch (L603–610).
- **Frame rate is ours now.** SDL has no frame cap. `VSync`/`Uncapped` become Dawn present modes; a cap becomes our own
  wait on desktop and skipped animation frames on web. emdawnwebgpu asserts Fifo [emdawn L2592–2596], so
  `VSync` = `Uncapped` there, as `platform.hpp` already says. `SetTargetFPS` did this in raylib.
- ImGui: the port has `sdl3-binding` and `webgpu-binding` [vcpkg]. Our debug UI feeds ImGui through its own code, so this
  is optional.

## 2. GLFW on the same list

Three implementations, not one:

| | Desktop GLFW 3.5.1 | Emscripten `-sUSE_GLFW=3` (4.0.10) | `--use-port=contrib.glfw3` (3.4.0.20250305) |
|---|---|---|---|
| Comes from | vcpkg `glfw3` | `libglfw.js`; vcpkg's `glfw3` port on Emscripten is only this flag [vcpkg] | emsdk port; 3.5.1.20261109 on [em-main] |
| WebGPU canvas | yes (`GLFW_NO_API`) | yes: no context when `GLFW_CLIENT_API` is 0 (L1097–1113) | yes: no context with `GLFW_NO_API` (`Window.cpp` L480); not run |
| Device pixel ratio | yes | with `GLFW_SCALE_TO_MONITOR`, and it reacts to ratio changes (L1400) | not checked |
| Fill the page | n/a | by hand; GLFW forces the CSS size when high-DPI is on (L1258–1263) | `MakeCanvasResizable(window, "window")` |
| Pointer lock | yes | yes; state not reported, re-locks on click | yes; mode follows the real lock (`Window.cpp` L656–700), better than SDL |
| Raw motion | `GLFW_RAW_MOUSE_MOTION` | "not implemented" (L977), same on main | "not supported" (`Window.cpp` L627–630) |
| Text | char callback | `keypress` charCode | first UTF-16 unit of `key`, so no characters above U+FFFF (lib JS L111) |
| IME composition | no API (`glfw3.h` has none) | none | none |
| Key repeat | `GLFW_REPEAT` | yes (L393–404) | yes |
| Fullscreen | `glfwSetWindowMonitor` | `glfwSetWindowMonitor` **throws** (L1752) | not implemented; own `RequestFullscreen` call |
| Gamepad hot-plug | joystick callback | joystick callback (L735–768) | joystick callback |
| Gamepad mapping | `glfwGetGamepadState` | `glfwJoystickIsGamepad` **throws** (L1891); **no `glfwGetGamepadState`**, raw axes and buttons only | `glfwGetGamepadState` for browser-standard pads, with analog triggers |
| Rumble | **none** | none | none |
| Wheel | float | quantized to at least ±1 (L600–602) | not checked |

raylib hides the bundled port's gaps with its own HTML5 calls (gamepads, pointer lock, mouse motion)
[raylib `rcore_web.c`]. Using GLFW directly means writing those again. A GLFW choice would be desktop GLFW + the contrib
port. It has the better pointer-lock state, but no rumble, its web half isn't in vcpkg, and it also prevents the browser's
default for every key it knows unless a browser-key callback says otherwise (`Keyboard.cpp` L100–118).

## 3. miniaudio on web

- **ScriptProcessorNode by default.** AudioWorklets only with `MA_ENABLE_AUDIO_WORKLETS` and
  `-sAUDIO_WORKLET=1 -sWASM_WORKERS=1 -sASYNCIFY` [ma L505–540, L41705]. Still the case in 0.11.26. Chrome logs the
  deprecation warning and plays [probe].
- **Cross-origin isolation:** none for the default path (`crossOriginIsolated = false` and it played [probe]). Worklets go
  through Wasm Workers, which need COOP+COEP [em-main `wasm_workers.rst` L449–451], and GitHub Pages deploys can't set
  those headers (`docs/research/procedural-audio.md`). Worklets also need Asyncify here, because `ma_device_init` spins on
  `emscripten_sleep` (L42260). A worklet path with neither exists from Emscripten 4.0.23 (`-sENVIRONMENT=worklet`)
  [em-main ChangeLog], but miniaudio doesn't use it. The plan should keep ScriptProcessor.
- **User-gesture unlock:** still only `touchend` and `click` (L42582–42607). Key presses did not start audio, a click did
  [probe, without our shim]. **Keep `tools/wasm_audio_unlock.js`**; it patches `AudioContext`, not raylib, so it carries over
  unchanged. While locked the callback isn't called: the music cursor and the synth stayed at 0 [probe].
- **Latency:** period 2048 frames at 48 kHz, 43 ms [probe], the same as the raylib build, since the 33 ms default rounds up
  to a power of two (L41895–41925). New: we own the device config, so `periodSizeInFrames = 1024` (21 ms) is one line,
  which raylib never exposed. Dropouts at 1024 under our frame times are unmeasured. Desktop default stays 10 ms × 3.
  Output latency to the speaker was not measured.
- **Thread model:** the callback runs on the main thread between frames, as today. A long frame still glitches audio.

### API against what `Platform` needs

Recommendation: `ma_engine` with one `ma_sound` per loaded sound or track.

| Platform call | miniaudio | Note |
|---|---|---|
| `LoadSound` | `ma_sound_init_from_file` with `MA_SOUND_FLAG_DECODE` | default VFS is `fopen`, works on MEMFS [probe] |
| `PlaySound` | `ma_sound_seek_to_pcm_frame(0)` then `ma_sound_start` | `ma_sound_start` **does nothing while playing** (L79020); raylib restarts [raylib `raudio.c` L645–655], so the seek is needed for parity. Overlapping plays would need `ma_sound_init_copy`, a later improvement |
| `SetSoundPitch`, `SetSoundVolume` | `ma_sound_set_pitch` / `_set_volume` | per sound, atomic (L79164); pitch ≤ 0 is ignored |
| `LoadMusic` | `MA_SOUND_FLAG_STREAM` | about 2 s buffered (L1309); not shared between loads, so keep our path map |
| `PlayMusic`, `StopMusic` | seek 0 + `set_looping` + start; stop | `ma_sound_stop` keeps the position |
| `UpdateMusic` | nothing on desktop (job thread); on web `ma_resource_manager_process_next_job` | on web the engine sets `NO_THREADING` (L78006) and already runs one job per callback (L77766), which was enough to stream [probe]. The virtual can go |
| `StartSynth` | custom `ma_data_source` (f32, mono, 48 kHz) wrapped in a `ma_sound` | user data comes with the object, so the global and the static trampoline go |
| `StopSynth` | `ma_sound_uninit` | same promise: detaching "will wait for any local processing of the node to finish" (L74977–74983) |

`SynthSource` survives unchanged: `Render` got 2048-frame blocks on web [probe], and its "any frame count" contract covers
desktop. miniaudio's mixer takes no global mutex around `Render` (the node graph uses atomics and spin waits), so a slow
`Render` no longer blocks `PlaySound`, but `StopSynth` waits for it. Set `ma_engine_config.sampleRate = 48000`
so the synth isn't resampled; the web device honors it [probe: `rate=48000`]. Spatialization is off with
`MA_SOUND_FLAG_NO_SPATIALIZATION`.

### Decoders

All 20 audio files in the repo were checked by header bytes: 17 WAV (PCM, mono, 44.1 kHz; `click.wav` and `jump.wav` 8-bit,
the rest 16-bit) and 3 Ogg **Vorbis** (stereo, 44.1 kHz: crabnado `music`, `title`, `gameover`). miniaudio decodes WAV, MP3 and
FLAC itself; Vorbis is on when `stb_vorbis.c` is included header-only before the implementation and fully after it
(L65545). vcpkg's `stb` port installs `stb_vorbis.c` [vcpkg]. Both formats loaded [probe]. `MA_NO_FLAC`, `MA_NO_MP3` can be set.
raylib also read QOA, XM and MOD; no file uses them.

### Build notes

- miniaudio on Emscripten can't be compiled with `-std=c*` or `-ansi` [ma L518]. Compile it in its own TU. CMake uses
  `-std=gnu++23` by default and the project sets no `CMAKE_CXX_EXTENSIONS OFF`, but `project()` enables CXX only, so either
  use a `.cpp` TU or enable C [local `CMakeLists.txt` L2–5]. A C TU is what the probe built; a C++ TU is not confirmed.
- **Clash with raylib while both are linked.** raylib's static library exports `ma_device_init`, `ma_context_init` and
  `stb_vorbis_open_memory` (checked with `llvm-nm` on the wasm `libraylib.a`), as our TU would. Phase 2's "behind a CMake
  option beside raylib" must select one or the other per build, not link both; or port audio only when raylib leaves.
- SDL3's own audio (`sdl3-mixer`) isn't needed: SDL's web audio is also ScriptProcessorNode [sdl `SDL_emscriptenaudio.c`
  L259], and vcpkg's `sdl3-mixer` adds libvorbis and more dependencies.

## 4. Saves, file paths and assets on web

- **Saves use `localStorage`, through five `EM_JS` functions in `raylib_backend.cpp`** (`js_write_save`, `js_read_save`,
  `js_has_save`, `js_delete_save`, `js_list_saves`, prefix `game_save_`). They are **neither raylib calls nor Emscripten FS
  calls**, so they move to `SaveStore` as they are. Desktop and headless use `FileSaveStore`, with no raylib either
  [local `raylib_backend.cpp` L41–78, L2609–2690; `file_save_store.cpp`].
- Two weaknesses to carry over knowingly: `setItem` has no `try`/`catch`, so a full or blocked store throws through wasm;
  and data passes as a C string, so an embedded NUL ends it.
- SDL's alternative is a compile-time IDBFS mount (`SDL_EMSCRIPTEN_PERSISTENT_PATH`, plus `-lidbfs.js`) that writes back over
  "the next few iterations" [sdl README-emscripten]. The vcpkg portfile doesn't set it, and it loses the synchronous write.
  Keep `localStorage`. `SDL_GetPrefPath` without it is `/libsdl/…` in MEMFS, lost on reload [sdl `SDL_sysfilesystem.c` L45].
- **Asset directory:** today `GetApplicationDirectory()` + `assets` on desktop and the literal `/assets` on web [local
  L1399–1405]. `SDL_GetBasePath()` is the executable's directory on desktop and `"/"` on web [sdl `SDL_sysfilesystem.c` L35–38; probe], so
  `SDL_GetBasePath() + "assets"` is right on both with no `#ifdef`, except inside a macOS `.app`, where it is the
  Resources folder (`SDL_filesystem.h`). `FileSaveStore` already has its own `ExecutableDirectory()` for code with no
  window, so `HeadlessPlatform` stays free of SDL.
- `--preload-file … @/assets` into MEMFS is Emscripten's, not raylib's, and stays. `-sUSE_GLFW=3` and
  `-sGL_ENABLE_GET_PROC_ADDRESS=1` in `GameTarget.cmake` go with raylib.

## 5. PLAN.md's claims

| Claim | Verdict |
|---|---|
| SDL3 for window/input | **Holds**, with the web shims above |
| SDL3 for IME | **Half wrong.** Desktop yes; web has no composition and no mobile keyboard |
| "gamepads come almost free" | **Holds.** Better than raylib on both targets. Browser-unmapped pads are still guessed |
| "SDL3 over GLFW for gamepad hot-plug" | **Wrong reason.** GLFW has hot-plug callbacks on desktop and in both web ports |
| "… and rumble" | **Holds.** GLFW has none, anywhere |
| "… text input" | **Mostly wrong as stated.** Both deliver characters; SDL3's gain is desktop IME, and it adds a start/stop call |
| "… and one web implementation" | **Holds** if the vcpkg port is used. Emscripten ships a second, older SDL3 (3.2.4 in our emsdk), and GLFW has two web ports |
| "miniaudio + stb_vorbis for audio (keeps `SynthSource`)" | **Holds**, and it removes the global trampoline |
| "SDL_GPU (no web)" | **Holds.** 3.4.20 has only `d3d12`, `metal` and `vulkan` under `src/gpu` |
| "Local emsdk 4.0.10" is enough | **Holds for SDL3, miniaudio and emdawnwebgpu's port** (built and run). Emscripten is at 7.0.1 now |
| imgui has `webgpu-binding` | **Holds**; `sdl3-binding` too |
| Fallback on "GLFW/glad" | Possible, but on web it needs the contrib port or raylib's shims rewritten. SDL3 + WebGL 2 is the cheaper fallback |

## 6. Recommendation

1. **SDL3 from vcpkg on both targets.** Keep GLFW out.
2. Give the canvas a CSS size in `wasm_shell.html`; use `RESIZABLE | HIGH_PIXEL_DENSITY`, not `FILL_DOCUMENT`.
3. Write a small web file beside the SDL backend: pointer-lock status, fullscreen guard and status, ratio listener,
   key filter. About five functions, all `emscripten/html5.h` calls or `EM_JS`.
4. Add to `Input`: `raw` delta (#72), text-input start/stop, gamepads with events and rumble.
5. miniaudio with `ma_engine`, ScriptProcessor on web, keep the unlock shim, drop `UpdateMusic`. Build with **either** raylib
   **or** miniaudio.
6. `SaveStore` = today's `localStorage` functions + `FileSaveStore`. Not IDBFS.
7. Leave web IME out of the parity list; nothing has it today.

## 7. Coverage

"Yes" = the library does it on that target. "Ours" = our code, whichever library is used. D = desktop, W = web.

| Platform group | SDL3 3.4.20 | GLFW (desktop 3.5.1 / web ports) | miniaudio 0.11.26 |
|---|---|---|---|
| `Init`, `ShouldClose`, `Shutdown`, `HasWindow` | D yes, W yes | D yes, W yes | device init and shutdown only |
| `Width`, `Height` (logical canvas) | ours; gives window and pixel sizes | ours; same | – |
| `SetFullscreen`, `IsFullscreen` | D yes, W yes + denial guard | D yes; W bundled throws, contrib own API | – |
| `DeltaTime`, `Time`, `Fps` | `SDL_GetTicksNS`; `Fps` ours | `glfwGetTime`; `Fps` ours | – |
| `SetFrameRate` | ours (present mode, wait, skip) | ours | – |
| `AssetDirectory` | `SDL_GetBasePath`, both | ours | – |
| `BeginFrame`, `EndFrame` | events yes; present is the renderer's | same | – |
| `MousePosition`, `MouseWheel` | D yes, W yes; map to canvas ours | D yes, W yes (bundled wheel quantized) | – |
| `MouseDelta`, raw delta (#72) | D yes unaccelerated (X11 with vcpkg: no), W accelerated | D raw; W accelerated | – |
| `SetCursorLocked`, `IsCursorLocked` | D yes (X11 with vcpkg: no), W lock yes, **state ours** | D yes, W contrib yes, bundled state ours | – |
| `MousePressed/Released/Down`, `IsMouseButtonDown` | yes; edges ours | yes; edges ours | – |
| `IsKeyDown`, `IsKeyPressedRepeat` | D yes, W yes; filter browser shortcuts | D yes, W yes | – |
| `CharInput`, `TextInput` | D yes with IME, W yes without IME; start/stop needed | D yes without composition, W yes without IME | – |
| Gamepads, hot-plug (not in `Platform` yet) | D yes, W yes | D yes; W contrib yes, bundled raw only | – |
| Rumble (not in `Platform` yet) | D yes, W yes | no | – |
| `GetDebugUi`, `DrawDebugUi` | imgui `sdl3-binding` available | imgui `glfw-binding` available | – |
| 2D, 3D, batching, shaders, meshes, textures, animation | no (renderer) | no | – |
| `LoadSound` … `SetSoundVolume` | no (`sdl3-mixer` would) | no | D yes, W yes |
| `LoadMusic` … `SetMusicVolume` | no | no | D yes, W yes; `UpdateMusic` not needed |
| `StartSynth`, `StopSynth` | no (SDL audio streams would) | no | D yes, W yes |
| `WriteSave` … `ListSaves` | no (IDBFS option rejected) | no | ours, unchanged |

## Not verified

- Pointer lock and fullscreen **succeeding** through SDL in a browser, and the feel of `movementX` on high-DPI displays.
- Firefox, Safari, mobile browsers, touch. Real multi-monitor ratio changes.
- Any gamepad on hardware, including rumble and hot-plug.
- SDL3 and miniaudio on desktop: read from source, not run. Their `x64-windows` DLLs add SDL3.dll to packaging.
- The vcpkg `sdl3` port built through vcpkg for `wasm32-emscripten`, its CI time, and SDL3's own share of the wasm size.
- Whether ScriptProcessor at 1024 frames holds under our worst frames.
- Both GLFW web ports were read, not run.
