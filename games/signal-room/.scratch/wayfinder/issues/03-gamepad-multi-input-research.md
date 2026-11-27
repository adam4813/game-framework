# Gamepad & per-Operator input through Platform

Map: [Signal Room: Vertical Slice](../map.md)
Label: wayfinder:research
Type: research
Status: resolved
Blocked by: 

## Question

What does Raylib (the vcpkg version this repo pins) offer for gamepads: count, hot-plug, button/axis mapping, standard layout naming, WASM/browser Gamepad API behaviour, and what would the engine's `Platform` + `InputState` (currently keyboard + mouse, a single singleton) need in order to support several independent input devices, each bound to an Operator? Capture options and their costs; don't design the game-side binding (that's 'Operators & Swap').

## Answer

Findings: `docs/research/gamepad-multi-input.md` (folded into main with 0.7.0;
the research branch is gone).

- raylib 5.5 (vcpkg baseline): max 4 gamepads, so keyboard + 3 pads at most. Buttons are positional (W3C standard
  layout). No hot-plug events: poll `IsGamepadAvailable` each frame and diff.
- Desktop and web behave differently: triggers are axes on desktop but digital-only on web; Guide is dead on web;
  vibration works on web only; custom mappings work on desktop only. No deadzone on either, so the engine must apply
  one.
- On web, a pad is invisible until a button is pressed (needs a "press a button to join" UX). Pad indices can be
  reassigned on reconnect, so Operator bindings must re-bind.
- A single keyboard is one device: a split keyboard (WASD / arrows) is a binding concern, not a device.
- The engine has no gamepad support today. `InputState` must stay a POD copy (scripts take it by value; the
  override-input tool and the null platform depend on it).
- Recommended: **A + C**. Add a raw pad array to `InputState` plus a Platform pad-snapshot call with engine-owned
  enums and connect/disconnect events. On top, add a data-driven action-mapping layer that resolves bindings into
  per-Operator action state. Treat triggers as digital and don't rely on Guide or rumble for gameplay.
- Caveats: two upstream raylib 5.5 out-of-bounds bugs only bite above 4 devices. Unverified: whether gamepad access
  needs a secure context, and XInput/DirectInput duplicates on Windows.
