# Phase 1 — Foundation

**Status**: ⬜ Not Started
**Depends on**: — (none)
**Spec**: [spec](../spec.md)

## Goal

Stand up the data-driven pipeline and the new Football scene so later phases have solid ground: data types,
a JSON loader → registries, tuning constants, a Flecs module, and title→Football wiring. No gameplay yet.

## Desired Final State (Guiding Light)

> **When Phase 1 is done, the game boots, you click "Football" on the title, and a placeholder Football
> scene appears whose data came entirely from JSON on disk — not from hardcoded C++.** The loader reports
> the exact number of plays/modifiers/scenarios it read, the Flecs Explorer shows the football components,
> and returning to Title works. Nothing is fun yet — but the *spine* (data → registries → scene) is proven,
> so Phase 2 can add behavior without touching plumbing.

**Measures of success** (the GATE checks these):
- Adding a new play/modifier/scenario is a **JSON file drop** — zero C++ changes to be picked up by the loader.
- Loader counts equal the file counts, with **no parse warnings**.
- The Football scene activates and deactivates cleanly (no leaks, no stuck state) alongside the untouched
  pinball scenes.
- Data struct field names read naturally and are stable enough that Phase 2 won't need to rename them.

**Explicitly NOT in scope**: any odds math, resolution, drive rules, or UI beyond a placeholder panel.

## Tasks

> Guidelines, not a script — shape the code as the engine prefers. Every task lists a `Done when:` measure;
> if a task fights the Desired Final State, change the task. Use the **flecs-systems** skill for the module.

1. **Tuning constants** — `games/football/src/game/football/football_constants.hpp`.
   - Gather every gameplay number the phase implies as `constexpr` in one place: field length (100),
     first-down yards (10), downs per set (4), quarters (4), starting yard line (25), default hand size,
     default RNG seed.
   - Prefer a small `namespace game::football::constants` (or an inline `constexpr` struct) over scattered
     literals. Add brief comments only where a number's meaning isn't obvious.
   - *Done when:* the header compiles standalone and no other Phase-1 file introduces a magic gameplay number.

2. **Play / modifier / scenario data structs** — `games/football/src/game/football/components/data_defs.hpp`.
   - `PlayDef`: `id`, `name`, `category` (enum or string: run/pass/trick), `tags` (`std::vector<std::string>`),
     `baseSuccess` (float), `yardBuckets` (`std::vector<{int yards; float weight;}>`), `turnoverChance` (float).
   - `ModifierDef`: `id`, `name`, `effect` (handler-key string), `params` (`std::unordered_map<std::string,
     float>` or a small variant), optional `condition` (string, parsed later), `stackable` (bool).
   - `ScenarioDef`: `id`, `name`, `oddsCurve` (deltas keyed by field-zone × down — pick a simple nested map
     or flat table), `lossCondition` (enum: `turnover`/`opponentScore`/`clock`), `opponentPace`.
   - Keep these **plain data** — no methods, no logic (engine convention).
   - *Done when:* structs compile, are trivially constructible, and express the whole Design-Decisions schema.

3. **Drive / game state components** — `games/football/src/game/football/components/drive_components.hpp`.
   - `DriveState` singleton: `down`, `yardsToFirst`, `ballYard`, `possession` (enum player/opponent).
   - `GameState` singleton: `quarter`, `homeScore`, `awayScore`, `clock`, `Phase phase`.
   - `Phase` enum: `PlaySelect`, `Resolving`, `Result`, `QuarterEnd`, `GameOver` (the state machine's states).
   - *Done when:* singletons default-construct to a sane pre-game state (down 1, ball at start yard, PlaySelect).

4. **Playbook / odds components** — `playbook_components.hpp` + `odds_components.hpp`.
   - `Playbook` (owned play ids/defs), `Hand` (current selectable plays), `AttachedModifier` (modifier id +
     target).
   - `OutcomeDistribution` (the sampled space), `OddsResult` (`successPct`, `yardMin`, `yardMax`,
     `yardExpected`, `turnoverPct`), `RngState` (wraps a seeded `std::mt19937`).
   - *Done when:* headers compile; `RngState` is seedable and copyable enough for deterministic replay later.

5. **Registries** — `games/football/src/game/football/data/registries.hpp`.
   - `PlayRegistry`, `ModifierRegistry`, `ScenarioRegistry` singletons: `id → def` maps + a `Get(id)` that
     returns a pointer/optional and warns on miss.
   - *Done when:* a registry can be populated and queried by string id in a unit-sized smoke test or at load.

6. **JSON data loader** — `games/football/src/game/football/data/data_loader.{hpp,cpp}`.
   - Use **nlohmann-json**. Resolve the data root from `Platform::AssetDirectory()` + `/data/{plays,
     modifiers,scenarios}`; iterate files in each dir; parse into defs; insert into registries.
   - Be defensive: missing dir/file or bad JSON → `spdlog::warn` and skip, never crash. Log a summary line
     `[football] loaded N plays / M modifiers / K scenarios`.
   - Factor a tiny `ParsePlay/ParseModifier/ParseScenario` per type so schemas are easy to evolve.
   - *Done when:* pointing it at `assets/data/**` populates all three registries with correct counts and no throws.

7. **Flecs module** — `games/football/src/game/football/football_module.{hpp,cpp}` (`FootballModule`).
   - Register football components with Flecs meta (for the Explorer). Provide empty
     `RegisterFootballSystems()`-style hooks the later phases fill in — keep registration in dedicated
     functions per convention.
   - *Done when:* `world.import<game::football::FootballModule>()` succeeds and components show in the Explorer.

8. **Football scene** — `games/football/src/game/football_scene.{hpp,cpp}` (`engine::scene::Scene` subclass).
   - Mirror `GameScene`: `InitializePipeline` (copy the running/paused pipeline build), `Load()` imports the
     module, runs the loader, and draws a placeholder imgui panel; implement `Unload`, `HandleInput`, `Tick`.
   - Expose `onQuitPressed`. Route any 2D drawing through `Platform` (no direct Raylib).
   - *Done when:* activating the scene shows the panel and `Unload` leaves no dangling entities/systems.

9. **Scene registration + title wiring** — `scene_components.hpp` + `game.cpp`.
   - Add `Football` to `engine::scene::SceneId`. In `Game::Init()` construct `FootballScene`, set
     `onQuitPressed = [world]{ ActivateScene(Title); }`, `RegisterScene(SceneId::Football, ...)`, and add a
     "Football" title button that activates it.
   - *Done when:* Title ↔ Football navigation works both directions at runtime.

10. **Starter JSON data** — `assets/data/{plays,modifiers,scenarios}/*.json`.
    - Plays (≥3): `short_pass`, `inside_run`, `deep_pass` with distinct `yardBuckets`/`baseSuccess`/
      `turnoverChance`. Modifiers (2): `short_boost` (`effect: add_success`, `condition: tag==short`),
      `safe_hands` (`effect: reduce_turnover`). Scenarios (2): `sandlot_easy` (`lossCondition:
      opponentScore`) and `blitz_hard` (`lossCondition: turnover`) with different `oddsCurve` deltas.
    - *Done when:* all files parse and appear in the loader summary; they're sufficient input for Phase 2.

## Verification Checklist

- [ ] Desktop build: `cmake --preset desktop && cmake --build --preset desktop-release`.
- [ ] WASM build: `cmake --preset wasm && cmake --build --preset wasm-release`.
- [ ] Title → **Football** activates the scene; Quit returns to Title.
- [ ] Loader logs the correct `N/M/K` counts with **no** parse warnings.
- [ ] Placeholder panel renders; scene unloads cleanly.
- [ ] Football components visible in the Flecs Explorer (localhost:27750).
- [ ] Files follow `AGENTS.md` style (snake_case, tabs, PascalCase types); one concern per file.

## 🛑 GATE

Measured against the Desired Final State. Ask the human:
- **Does Football load and return to Title cleanly?**
- **Do loader counts match the JSON on disk (no warnings)?**
- **Is adding content truly a JSON-only drop (spot-check by adding one play file)?**
- **Are the data schemas right, or should fields change before Phase 2 builds on them?**
- **Any blocking issues?** (true = fix now; false = note in Feedback)
- **Additional feedback?** (free text)

## Git Checkpoint

**Stage** (only these): `games/football/src/game/football/**`, `games/football/src/game/football_scene.*`, `games/football/src/game/game.cpp`,
`src/engine/scene/scene_components.hpp`, `assets/data/**`. Verify with `git diff --cached --stat`.

**Commit**: `feat: football foundation (data types, json loader, registries, scene skeleton)`
(+ `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`)

## Feedback

_(none yet)_
