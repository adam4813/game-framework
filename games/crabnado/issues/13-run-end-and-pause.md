# 13: Run end and pause

Spec: [spec](../spec.md). Read: [The run](../spec/run.md) (death, results, pause).

**What to build:** A run ends cleanly. At 0 HP a results modal over the game scene shows time survived, kills and molt
level, with Retry (reloads the game scene in place) and Title buttons. Esc opens a pause menu (pause as data) with
Resume and Quit to title. When the boss ticket lands, victory uses the same results modal with a win heading.

**Blocked by:** 02 (Core loop).

**Status:** ready-for-agent

- [ ] Dying shows the results modal with time, kills and molt level.
- [ ] Retry starts a fresh run (clock, HP, horde and upgrades reset) without going through the title.
- [ ] Title returns to the title scene.
- [ ] Esc pauses enemies, the crabnado, waves and the clock, and Resume continues exactly where it left off.
- [ ] If ticket 12 has landed, winning shows the results modal with a victory heading.
- [ ] The scene-flow test still passes.
- [ ] Checked in the running game (e.g. set HP to 0 over the Remote API, then click Retry).

## Decisions

Settled 2026-10-24.

- **Retry** calls the engine's `scene::ReloadScene<ArenaSceneTag>(world)` (Unload → Load in place). `StartRun`
  already reseeds, so check that player stats, upgrades (they live on the player entity, which is recreated), wind,
  the wave field and the horde all reset. Check that `scene::Paused` is cleared on Load (`arena_scene.cpp` already
  removes it).
- **Results modal.** A retained UI modal named `ResultsModal`, with buttons `RetryButton` and `ResultsTitleButton`
  (names the Remote API can click). It shows the heading ("Swept away" or "The crabnado breaks apart!"), time
  survived (mm:ss), kills, rock slams (if ticket 08 has landed) and molt level. It replaces the placeholder label.
- **Pause.** Esc is a `pause` action in `PlayerIntent` (pressed this frame). Track Esc-pause as
  `RunState.menu_paused` (bool, reflected), **separate from `phase`**, so pausing during the boss resumes the boss.
  Esc toggles it only while the phase is `Playing` or `Boss`. It's ignored while molting, lost or won. The pause menu
  is a modal `PauseModal` with `ResumeButton` and `QuitButton` (Quit goes to the title). Esc on the menu also
  resumes. It uses `scene::Paused` like molting.
- **The HUD's top-right Title button stays.** The scene-flow test uses it, and keeping it is cheaper than moving the
  test.
