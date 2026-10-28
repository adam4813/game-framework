# Research: Gamepad & per-Operator input through Platform

Ticket: "Gamepad & per-Operator input through Platform" (Signal Room, wayfinder research).
Question: what Raylib (pinned version) gives us for gamepads on desktop and WASM, and what `Platform` +
`InputState` would need to support keyboard + 1–3 gamepads, each driving a different Operator (local co-op).
Out of scope: the game-side binding/Swap rules (separate grilling ticket).

Date: 2026-10-17. All source line numbers refer to the exact sources listed below.

## Sources (primary)

| Ref | Source |
|-----|--------|
| [vcpkg] | `vcpkg.json` → `builtin-baseline: dd3097e3…`; `versions/baseline.json` at that commit: **raylib 5.5 (port-version 0)**, **glfw3 3.4 (port-version 1)** |
| [port] | `ports/raylib/portfile.cmake` @ baseline: desktop = `-DPLATFORM=Desktop -DUSE_EXTERNAL_GLFW=ON` (vcpkg glfw3 3.4), emscripten = `-DPLATFORM=Web -DUSE_EXTERNAL_GLFW=OFF` |
| [rl.h] | raylib 5.5 `src/raylib.h` L724–754 (enums), L1183–1193 (API) |
| [cfg] | raylib 5.5 `src/config.h` L82–85 |
| [rcore] | raylib 5.5 `src/rcore.c` L3240–3325 (gamepad query functions) |
| [glfw-be] | raylib 5.5 `src/platforms/rcore_desktop_glfw.c` L1085–1095, L1174–1249, L1422–1426, L1651–1659, L1916–1927 |
| [web-be] | raylib 5.5 `src/platforms/rcore_web.c` L886–921, L999–1064, L1356–1358, L1700–1719 |
| [glfw] | GLFW 3.4 `include/GLFW/glfw3.h` (`GLFW_JOYSTICK_LAST = GLFW_JOYSTICK_16`, `glfwGetGamepadState` doc), `src/mappings.h` (bundled SDL_GameControllerDB snapshot, ~1000 lines), `src/win32_joystick.c` (XInput + DirectInput) |
| [em] | Emscripten 4.0.10 (local `$EMSDK`) `system/include/emscripten/html5.h` L370–390, `src/lib/libhtml5.js` L2010–2067 |
| [w3c] | W3C Gamepad spec, https://w3c.github.io/gamepad/ (editor's draft) |
| [local] | This repo: `src/engine/platform/platform.hpp`, `src/engine/input/input_components.hpp`, `src/engine/input/input_module.cpp`, `tools/flecs-api.js` L73–80/L285–296, `tests/scene_equivalence/null_platform.hpp` |

## 1. What Raylib 5.5 offers

### API surface [rl.h L1183–1193]

```
bool  IsGamepadAvailable(int gamepad);
const char *GetGamepadName(int gamepad);
bool  IsGamepadButtonPressed/Down/Released/Up(int gamepad, int button);
int   GetGamepadButtonPressed(void);          // last button pressed on ANY pad
int   GetGamepadAxisCount(int gamepad);
float GetGamepadAxisMovement(int gamepad, int axis);
int   SetGamepadMappings(const char *mappings); // SDL_GameControllerDB format
void  SetGamepadVibration(int gamepad, float left, float right, float duration);
```

- No connect/disconnect callback or event is exposed to the user. Hot-plug must be detected by polling
  `IsGamepadAvailable(i)` each frame and diffing [rcore L3242–3248].
- `GetGamepadButtonPressed()` is global (one `lastButtonPressed` across all pads) [rcore, glfw-be L1230],
  so it cannot tell you *which* pad was pressed. A "press any button to join" flow has to scan each pad.

### Count [cfg L82–85]

- `MAX_GAMEPADS 4`, `MAX_GAMEPAD_AXIS 8`, `MAX_GAMEPAD_BUTTONS 32`, `MAX_GAMEPAD_VIBRATION_TIME 2.0f`.
  These are compile-time constants in raylib's `config.h`. The vcpkg port does not override them, so
  **4 pads at most**. That covers keyboard + 3 gamepads exactly.

### Standard layout naming [rl.h L724–754]

Raylib names buttons by **position**, not by label, which matches the W3C "standard gamepad":

- `LEFT_FACE_{UP,RIGHT,DOWN,LEFT}` = D-pad; `RIGHT_FACE_{UP,RIGHT,DOWN,LEFT}` = Y/B/A/X (Xbox) or △/○/✕/□ (PS)
- `LEFT_TRIGGER_1/2`, `RIGHT_TRIGGER_1/2` = bumper / trigger
- `MIDDLE_LEFT` (Back/Select), `MIDDLE` (Guide/PS), `MIDDLE_RIGHT` (Start), `LEFT_THUMB`, `RIGHT_THUMB`
- `GAMEPAD_BUTTON_UNKNOWN = 0`, so the real buttons are 1..17.
- Axes: `LEFT_X=0, LEFT_Y=1, RIGHT_X=2, RIGHT_Y=3, LEFT_TRIGGER=4, RIGHT_TRIGGER=5`. Header comment says
  the trigger range is `[1..-1]`.

### Desktop backend (GLFW 3.4, Windows) [glfw-be]

- Each frame, `PollInputEvents` sets `ready[i] = glfwJoystickPresent(i)` for i < 4 (L1176–1180), so a
  disconnect shows up on the next frame.
- It reads `glfwGetGamepadState(i, &state)` (L1192–1193). GLFW remaps every device to an Xbox-like layout
  using its **bundled SDL_GameControllerDB** [glfw `mappings.h`]. Raylib then translates GLFW buttons
  to its own enum (L1199–1221).
- **Pitfall: an unmapped joystick counts as "available" but always reads zero.** `glfwJoystickPresent` is
  true for any joystick, but `glfwGetGamepadState` returns `GLFW_FALSE` and leaves the state zeroed when
  the device has no gamepad mapping [glfw doc for `glfwGetGamepadState`]. Raylib ignores that return
  value. `SetGamepadMappings()` → `glfwUpdateGamepadMappings` is how you add mappings for odd devices
  (L1086–1089).
- Trigger axes are raw GLFW values: **−1 at rest, +1 fully pressed**. Raylib turns them into digital
  `*_TRIGGER_2` buttons with a threshold of `> 0.1` (L1245–1246).
- Axis count is always 6 (L1248).
- `SetGamepadVibration` is **not implemented** on desktop. It only logs a warning (L1092–1095).
- The connect callback copies the name into `name[jid]`, which has 4 slots, while GLFW allows jid up to 15.
  A 5th device can write outside the array (L1916–1927 vs `char name[MAX_GAMEPADS][64]` in rcore L348).
  This is an upstream bug, worth knowing about but very unlikely to matter with ≤4 devices.
- Slot = GLFW joystick id. GLFW gives a freed id back to the next device that connects, so the slot can
  change if a pad is unplugged and replugged. Windows XInput pads and DirectInput pads share the 16 ids
  [glfw `win32_joystick.c`].
- Startup: raylib forces GLFW 3.4's lazy joystick init before showing the window (L1422–1426), so the
  first-frame hitch is avoided.

### Web backend (Emscripten → browser Gamepad API) [web-be, em, w3c]

- Each frame: `emscripten_sample_gamepad_data()` (which calls `navigator.getGamepads()`), then
  `emscripten_get_num_gamepads()`, then `emscripten_get_gamepad_status(i)` for i < min(n, 4)
  [web-be L1002–1013; em libhtml5.js L2026–2066].
- `ready[]` is set **only** by the `gamepadconnected`/`gamepaddisconnected` callbacks
  (L1356–1358, L1712–1717). The disconnect branch writes `ready[index]` without a bounds check, so a
  disconnect with index ≥ 4 is out of bounds (upstream bug).
- Raylib maps buttons by **W3C standard indices 0–15** (L1022–1040) and **does not check
  `gamepad.mapping == "standard"`**. A browser-unmapped pad (`mapping == ""`) has arbitrary button/axis
  order, and Raylib will report wrong buttons for it.
- Button 16 (Guide/"center button") **is not mapped**, so `GAMEPAD_BUTTON_MIDDLE` never fires on web.
- **Triggers differ from desktop.** In the W3C standard mapping, triggers are *buttons 6/7 with an analog
  value*, and there are only 4 axes [w3c]. Raylib web sets `LEFT/RIGHT_TRIGGER_2` from `digitalButton[6/7]`
  and does not copy `analogButton`. As a result `GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_TRIGGER)`
  always returns 0 on web (the axis state is never written, and the −1 default is replaced by the zeroed
  state, rcore L3316–3321), while on desktop it is −1..1. **Analog triggers cannot be used in a way that
  behaves the same on both platforms through raylib 5.5.** Treat triggers as digital, or read them in
  our own web shim.
- `SetGamepadMappings` is a no-op on web (L887–890). Vibration **is** implemented on web through
  `vibrationActuator.playEffect('dual-rumble')` (L895–921).
- Browser rules [w3c]:
  - `getGamepads()` returns an **empty list until a "gamepad user gesture"**: a button press or axis
    movement on that pad while the page has user attention. **A pad already plugged in is invisible
    until the player presses something.** Raylib only marks it `ready` when `gamepadconnected` fires,
    which is after that gesture. The UX has to say "press a button".
  - Indices are first-come, first-served from 0. The slot of a disconnected pad becomes `null`; indices of
    pads that stay connected never move; a new pad gets the lowest freed index. Emscripten returns
    `NO_DATA` for those null holes (libhtml5.js L2059–2063).
  - The spec sets no maximum number of pads. Raylib's limit of 4 still applies.
  - Permissions Policy feature `"gamepad"` (default `*`). If an embedding iframe (for example an itch.io
    host) disables it, `getGamepads()` throws `SecurityError`. Emscripten catches the exception and turns
    gamepads off for the session (libhtml5.js L2030–2035).
  - Haptics are rejected while the document is hidden.
- `EmscriptenGamepadEvent` carries `mapping`, `analogButton[64]`, `axis[64]` and `id` [em html5.h L370–381].
  Our own `EM_JS` / html5 shim could therefore fix the trigger, button 16 and mapping gaps if needed.
  Raylib 5.5 just does not use these fields.

### Deadzones

Raylib 5.5 `GetGamepadAxisMovement` applies **no deadzone**. It returns the raw axis value whenever it is
non-zero [rcore L3314–3325]. Stick drift has to be filtered in the engine (a `constexpr` tuning constant,
per AGENTS.md).

### Keyboards

Raylib exposes **one merged keyboard**. It cannot tell two physical keyboards apart. "Two players on one
keyboard" is therefore a binding concern (WASD vs arrows as two virtual devices), not a device concern.

## 2. What the engine has today [local]

- `Platform` (`platform.hpp`) has keyboard and mouse virtuals only: `IsKeyDown`, `IsKeyPressedRepeat`,
  `IsMouseButtonDown`, `MousePosition`, … There is no gamepad method. `KeyCode` is an engine-owned code
  space, and the raylib backend translates it (`raylib_backend.cpp` `case input::KeyCode::… return KEY_…`).
  This is the precedent for engine-owned gamepad enums.
- `InputState` is a single singleton holding `std::array<KeyState,1600> keys` + `MouseState`. It is
  **intentionally trivially copyable** because scripts take it by value (`input_components.hpp` comment
  on `TypedText`). Only `mouse` is reflected. Script methods `IsKeyDown`/`WasKeyPressed`/… are registered
  on it.
- `InputPoll` (PreUpdate) fills it from `PlatformRef`, and `InputResetFrameState` (PostUpdate) clears edge
  flags. Edge detection (`pressed`) is computed in the engine, not taken from raylib.
- Consumers read `world.get<InputState>()` directly and hard-code keys (e.g. `grid_movement.cpp` L57–85
  with WASD/arrows, `ui_systems.cpp`, scenes' `HandleInput(world, const InputState&)` in `scene-base.hpp`).
  ~21 files reference `InputState`. Nothing references gamepads.
- Tooling depends on the singleton's shape: `tools/flecs-api.js override-input` disables `InputPoll` and
  injects an `engine.input.InputState` value. `tests/scene_equivalence/null_platform.hpp` implements
  `Platform`, so it needs stubs for any new pure virtuals.

## 3. What Platform needs (any option)

Minimum additions, mirroring how `KeyCode` works:

1. Engine-owned `GamepadButton` / `GamepadAxis` enums in `engine::input`. Naming them positionally like
   raylib/W3C (`FaceDown`, `FaceRight`, `DpadUp`, `LeftShoulder`, `LeftTrigger`, `Start`, `Select`,
   `Guide`, `LeftStick`, …) keeps them device-neutral. The backend translates them, and `Gamepad_*`
   script constants are registered like `Key_*`.
2. Platform virtuals, either fine-grained or as one snapshot call:
   - Fine-grained: `IsGamepadAvailable(int)`, `GamepadName(int)`, `IsGamepadButtonDown(int, GamepadButton)`,
     `GamepadAxis(int, GamepadAxis)`, `MaxGamepads()`.
   - **Snapshot (fewer virtuals, easier null/test platform):**
     `GamepadSnapshot PollGamepad(int slot) const` returning `{connected, name, buttons bitset, axes[6]}`.
   - Optional: `SetGamepadMappings(string_view)` (desktop only, data-driven mapping DB in assets),
     `SetGamepadVibration(...)` (web only in 5.5; desktop no-op).
3. Normalisation in the backend or `InputPoll`: a deadzone constant; triggers exposed as **digital
   buttons only** (or 0..1 analog with a web shim) so both platforms behave the same; ignoring pads that
   report connected but are unmapped (desktop: check `glfwJoystickIsGamepad`, which raylib does not
   expose, so this needs either a backend `#ifdef` or accepting the "dead pad" case).
4. Hot-plug as **events**, not per-frame logic in game code (AGENTS.md "Event-Driven"): `InputPoll` diffs
   the `connected` flags and emits a Flecs event, e.g. `GamepadConnected{slot}` / `GamepadDisconnected{slot}`.
   Game systems such as join/Swap/pause-on-disconnect observe the event.
5. Slot identity is **not stable across unplug/replug** on either backend (GLFW reuses jids; W3C reuses
   the lowest freed index). Anything that binds a device to an Operator should keep a binding keyed by
   slot and be ready to re-bind on reconnect. Name + slot is the best heuristic available; raylib exposes
   no GUID.

## 4. Options for representing multi-device state

### A. Array of pads inside the `InputState` singleton

`InputState` gains `std::array<GamepadState, kMaxGamepads> gamepads;` with
`GamepadState{bool connected; std::array<KeyState, N> buttons; std::array<float, 6> axes;}`. The keyboard
stays as it is.

- Pros: smallest change (~1 file of structs plus loops in `InputPoll`/`InputResetFrameState`). Stays
  trivially copyable, so scripts can keep taking it by value. `override-input` keeps working and can
  inject pad state. Reflection is easy (`member("gamepads", …)`). Fixed size (4) matches raylib's limit.
- Cons: no notion of "player/Operator". Every consumer has to know which slot it is bound to, so that
  mapping lives somewhere else anyway. The keyboard is treated differently from pads (not a slot).
- Cost: low. About 1 day including Platform + raylib backend + null platform + script constants.

### B. Per-device entities (`InputDevice` entities with `KeyboardState` / `GamepadState` components)

`InputPoll` creates or destroys device entities on hot-plug. Operators reference a device through a Flecs
relationship (e.g. `(BoundTo, device)`), in line with the AGENTS.md relationship guidance.

- Pros: most Flecs-idiomatic. Hot-plug becomes entity lifecycle (observers fire for free). Keyboard and
  pads are symmetric. Binding is data on the Operator.
- Cons: breaks the single by-value script `InputState` model and `override-input` tooling (needs
  per-entity injection). Components must be pre-registered outside systems; per AGENTS.md, a lazy
  `world.component<T>()` inside systems crashes with `stage_count > 1`. Creating/deleting entities from
  `InputPoll` needs deferred-mode care. Every consumer (UI, scenes, grid movement) has to change how it
  looks up input. It also still does not solve "two players on one keyboard" without extra virtual-device
  logic.
- Cost: medium–high (touches ~20 consumers plus tooling).

### C. Action-mapping layer (on top of A or B)

Data-driven bindings (JSON: `action → {device kind, key/button/axis, modifiers}` per binding profile)
produce a per-Operator `ActionState` component, e.g. `{move: vec2, buttons: {primary, secondary, swap, …}
with pressed/held/released}`. Game systems read `ActionState` on the Operator entity and never read raw
devices.

- Pros: this is what per-Operator co-op actually needs. Gameplay code becomes device-agnostic. Keyboard
  split (WASD vs arrows) becomes just two profiles bound to the same physical keyboard. Rebinding and
  Swap rules become data (AGENTS.md data-driven rule). Deadzone and trigger normalisation live in one
  place. Raw `InputState` stays for UI/debug/text entry, so existing consumers do not change.
- Cons: a new module (bindings loader, resolver system, action-name registry). Risk of over-engineering
  if the action set stays tiny.
- Cost: medium. Mostly new code, with few changes to existing code.

### Comparison

| | A: array in singleton | B: device entities | C: action layer |
|---|---|---|---|
| Change to existing consumers | none | all (~20 files) | none (additive) |
| Script by-value `InputState` | kept | broken | kept (ActionState is a separate component) |
| `override-input` tooling | kept (extendable) | rework | kept; can add action override |
| Hot-plug modelling | diff + event | entity lifecycle | inherits from A/B |
| Per-Operator semantics | no | partial (device ↔ Operator link) | **yes** |
| Keyboard split for 2 players | no | no | **yes** (two profiles) |

## 5. Recommendation

**A + C.** Put raw device state in the singleton (A: `gamepads[4]` next to `keys`/`mouse`), add Platform
gamepad support through a snapshot call with engine-owned enums, and detect hot-plug in `InputPoll`
by diffing and emitting Flecs events. Then add a thin, data-driven action layer (C) that resolves bindings
into a per-Operator `ActionState` component. B adds churn without delivering what co-op needs; the
Operator ↔ binding link belongs on the Operator in C, not on device entities.

Platform-parity constraints to bake in now, so web and desktop behave the same:

- Max 4 devices total (raylib `MAX_GAMEPADS`), so plan for keyboard + 3 pads.
- Treat triggers as digital (or add a web shim for `analogButton[6/7]`). Do not rely on `Guide` (missing on
  web) or on vibration (missing on desktop) for gameplay.
- Engine-side deadzone constant.
- Web: show a "press any button on your controller" prompt, because pads are invisible until the user
  gesture.
- Slots can change on replug, so rebinding on reconnect must be part of the (separate) binding/Swap design.

## 6. Open items / not verified

- Whether Chrome/Firefox additionally require a secure context for `getGamepads()`. The W3C draft and
  the MDN `getGamepads` page do not state it, so not verified here. Serving over https/localhost avoids
  the question.
- How Windows maps 4 XInput + DirectInput duplicates of the same pad (GLFW filters XInput devices out of
  DirectInput; not exercised here). Needs a hardware test.
- Behaviour of the two upstream out-of-bounds writes (`name[jid]` on desktop, `ready[index]` on web) with
  more than 4 devices. Possibly fixed after 5.5; check before upgrading or patching.
