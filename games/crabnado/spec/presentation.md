# Presentation

Back to the [spec](../spec.md).

## User stories

1. As a player, I want a fixed, north-up, angled top-down camera that follows me, so that wind and wave directions
   stay readable.
2. As a player, I want the water to rise and fall visibly with the crests, so that I can read the waves at a glance.
3. As a player, I want enemies to flash when hit, so that I can see my weapons working.
4. As a player, I want a visible ring when the claw snap fires, so that I learn its range.
5. As a player, I want wind streaks moving across the screen, so that I can see the breeze.
6. As a player, I want a HUD with HP, stamina, the run clock, molt level and shell progress, a wind compass, and the
   boss slam count, so that I have what I need without clutter. Layout: HP top left; the shell bar top centre with
   the molt-level chip on its right; the compass and sea state top right; time and kills in a small lower-right
   panel; and the loadout lower left (a slot per weapon with its level and a shade over the cooldown still to run,
   under the dig-in bar, which dims while exhausted).
7. As a player, I want sound for claw snaps, bubble pops, crests, molting and the boss, plus a music loop, so that the
   game feels alive.
8. As a web player, I want audio to start after my first click, so that the browser doesn't block it.
8a. As a player, I want music and sound-effect volume sliders and mutes, from the title and the pause menu, kept
   between sessions, so that I can set the mix I like.
9. As a developer, I want an FPS and enemy-count readout in the debug UI, so that I can watch performance.

## Implementation decisions

- **Art is programmer art from engine primitives:** spheres, capsules, cubes, quads, tinted per instance. The prototype
  shows the look: flattened sphere body, two claw spheres, small leg boxes.
- **Water visuals:** a grid of tinted, raised quads whose height and colour each frame come from the wave field. Per
  instance tint doesn't split render batches, so the grid stays one draw call. A vertex-displacement shader is optional
  polish. **Cut fallback:** moving foam bars on flat water.
- **Hit flash** is a short per-instance tint change, not a material swap.
- **Bursts** (death puffs): the engine has no one-shot particle burst. Use a short-lived rate emitter, or skip it.
- **HUD and menus** use the engine's retained UI module (labels, progress bars, buttons, modal). The upgrade screen is
  a modal of three cards (icon, name, description, and each labelled stat as "before → after" with the change in
  green when it helps, red when it hurts) and a Confirm button. A click or 1/2/3 selects a card; Confirm or Enter
  takes it. The icons come from `tools/make_icons.py`; the card stat labels come from the upgrades JSON.
- **Wind streaks** are short line segments moving with the wind around the camera, as in the prototype.
- **Audio** is free SFX and one music loop, loaded through the assets module and kept in the game's assets.
  Licences must allow redistribution, and credits go in the game's README.
- **Settings** is a modal reached from a smaller button under Play and Credits on the title, and from the pause menu
  (between Resume and Quit to title, standing in for the pause menu while open; Esc closes both and resumes). It has
  a volume slider and a Mute box for the music and for the sound effects. Each change applies at once and is saved to the `crabnado_settings` save slot
  (`saves/` beside the executable on desktop, `localStorage` on the web), which loads at startup. A music track's own
  level is a `MusicLevel` component; the setting scales it into `Music::volume`.
