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
