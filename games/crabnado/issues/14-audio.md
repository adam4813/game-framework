# 14: Audio

Spec: [spec](../spec.md). Read: [Presentation](../spec/presentation.md) (audio).

**What to build:** The game gets sound. Free, redistributable SFX for claw snap, bubble pop, crest, molt and the boss
(as those features exist), plus one music loop, are loaded through the assets module from the game's own assets. In the
browser, audio starts after the first click or key press. A credits file lists every asset with its source and
licence.

**Blocked by:** 02 (Core loop).

**Status:** ready-for-agent

- [ ] Music loops during the run.
- [ ] Claw snaps make a sound, and each later sound (pop, crest, molt, boss) is wired as its feature lands.
- [ ] In the WASM build, audio starts after the first input and doesn't error before it.
- [ ] Every asset's source and licence is in the credits file, and each licence allows redistribution.
- [ ] Checked by playing on desktop and in the browser.
