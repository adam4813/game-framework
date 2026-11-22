# Changelog

Framework releases, newest first. Each entry opens with the version's highlight, then lists what changed by area.
Earlier releases (v0.1.0 to v0.10.0) are in the git tags. Game Targets carry their own `project()` version, and their
releases are noted under the Framework release they shipped with.

## 0.11.0 - 2026-11-22

**Highlight: a UI framework and event-driven scripting, with Crabnado 1.0 as the first game built on them.**

### Added

- **UI framework** (#111): `Image` widget with fit modes, atlas sub-rects and tint; `UIAnchor` anchored positioning
  resolved in `UILayout`; the `UICapture` singleton, so UI-owned pointer and keyboard input is withheld from gameplay.
- **`EventHost` for event-driven scripts** (#123): a named event table, ordered script slots, handlers checked at load,
  game commands queued from handlers and applied at a safe point, and `After()` timers. A throwing handler disables
  only its own script.
- **Script function calls** (#120, #123): `IScriptInstance` can list a script's functions and call one by name.
- **Crabnado 1.0.0** (#120): a three-Act campaign driven from `campaign.json` and `bosses.json`, bosses, and the wave
  ride, wind-up and charge-back enemy behavior.
- **Game Target versions**: every Game Target now declares `project(<name> VERSION ...)`, which `setup_game_target`
  turns into the `GAME_VERSION` define.

### Changed

- **Framework logging goes through `ecs::Log`** (#125, renamed from `WorldLog`); scripting, physics and level load
  were moved onto it.
- **Sandbox assets moved** (#126): the sandbox's levels, scripts, models, textures, AI and tilemap data now live in
  `games/sandbox/assets/`. The root `assets/` keeps only shaders, `config/profiling.json` and `audio/sfx/click.wav`.
  A game that read the old shared tileset or atlas needs its own copy.

### Fixed

- **A throwing script is disabled** (#122), instead of logging every frame. `self` in a script is its host entity, and
  `GetT`/`MutT` on a missing component warns once per entity and component.
