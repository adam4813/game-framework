# Save Module

JSON state save/load for the engine. A **schema-driven** system: the developer declares exactly which properties are
persisted and how each maps to and from ECS data. It is deliberately **not** a raw component dump — nothing is saved
unless it is registered.

Storage is intentionally out of scope: the module produces/consumes JSON (or a string) and the caller decides where it
lives (desktop file, browser `localStorage`/IDBFS, cloud, …).

## Structure

| File                   | Purpose                                                                   |
|------------------------|---------------------------------------------------------------------------|
| `save_components.hpp`  | Plain-data holders for the schema (`SaveField`, `EntitySaveType`, …).     |
| `save_registry.hpp`    | `SaveRegistry` singleton + fluent builders + `SaveToJson`/`LoadFromJson`. |
| `save_registry.cpp`    | Serialization logic (dotted-path get/set, entity rebuild).                |
| `save_module.hpp/.cpp` | Flecs module — registers the `SaveRegistry` singleton.                    |
| `save.hpp`             | Public umbrella header.                                                   |

## Two ways to bind (they mirror each other)

* **Member-pointer sugar** — type-checked, auto bidirectional via `nlohmann_json`.
* **Lambda escape hatch** — for computed / cross-component values.

Member types must be JSON-convertible (arithmetic, `bool`, `std::string`, containers, or a type with `nlohmann`
`to_json`/`from_json`). For anything else, use the lambda escape hatch.

## Usage

Register the schema once (typically during scene/module setup):

```cpp
auto& save = world.get_mut<engine::save::SaveRegistry>();

// Singleton component members -> nested JSON under "economy".
save.Singleton<Economy>("economy")
    .Member("tokens", &Economy::parlorTokens)
    .Member("points", &Economy::scorePoints);

// Computed / cross-component value via the lambda escape hatch.
save.Field("meta.playtimeSeconds")
    .Get([](const flecs::world& w) -> engine::save::Json { return w.get<RunClock>().seconds; })
    .Set([](flecs::world& w, const engine::save::Json& j) { w.get_mut<RunClock>().seconds = j; });
```

Save / load explicitly (e.g. from a menu action). Storage is the caller's job:

```cpp
const std::string text = engine::save::SaveToString(world);   // persist `text` however you like
engine::save::LoadFromString(world, text);                    // restore
```

## Adding tag-based entities to a save

Mark the entities you want persisted with a tag of your choosing, then register the tag's shape. Entities are
**ephemeral**: on load, all currently tagged entities are destroyed and recreated from the saved array (no stable ids /
matching).

```cpp
struct Persist {};                          // your save tag (a zero-size struct)

// Mark instances to persist (explicit, developer's choice):
world.entity().add<Persist>().set<Upgrade>({.level = 2});

// Describe how a persisted entity serializes:
save.Entities<Persist>("upgrades")
    .Member("level", &Upgrade::level)       // pick specific members, or
    .Component<Upgrade>("upgrade");         // whole component (needs nlohmann to_json/from_json)

// Escape hatch for full control:
save.Entities<Persist>("fx")
    .Serialize([](flecs::entity e) -> engine::save::Json { return {{"kind", "spark"}}; })
    .Deserialize([](flecs::entity e, const engine::save::Json& j) { /* rebuild from j */ });
```

## Registered systems

None. Saving/loading is invoked explicitly by game code via `SaveToJson` / `LoadFromJson`
(or the `*String` convenience wrappers). The module only registers the `SaveRegistry` singleton.

## Worked example

`src/game/meta_save_example.{hpp,cpp}` wires this module into game state end-to-end:

* Singleton member binding for `MetaProgress` (tokens / score / runs).
* Tag-based ephemeral entities for permanent unlocks (`PersistTag` + `UnlockedUpgrade`).
* Slot-based persistence through the **Platform save API** (`WriteSave`/`ReadSave`/`HasSave`/
  `DeleteSave`/`ListSaves`) — a file per slot on desktop, a `localStorage` entry on the web.
* Script-facing verbs (`SaveGame(name)`, `LoadGame(name)`, `DeleteSave(name)`, `AddParlorTokens`,
  `UnlockUpgrade`, …) exposed via `RegisterGlobalFunctionForScripts`, driven by
  `assets/scripts/save_demo.as`.
* A demonstrative save-management UI (`src/game/save_ui.{hpp,cpp}`): a **New Save** button plus a scrollable list of
  slots, each with **Load** and **Delete**, rebuilt live as slots change.

Run the Game scene: use the save panel (top-left) to create / load / delete named slots, or press
`1`/`2`/`3` to change progress, `F5` to quicksave, `F9` to reload.

## Storage (Platform save API)

The module itself only turns the world into JSON; **where that JSON lives is the Platform's job**.
`engine::platform::Platform` exposes a minimal key-value blob store keyed by slot name:

```cpp
platform->WriteSave("slot1", engine::save::SaveToString(world));
if (platform->HasSave("slot1")) {
    engine::save::LoadFromString(world, platform->ReadSave("slot1"));
}
for (const std::string& name : platform->ListSaves()) { /* ... */ }
platform->DeleteSave("slot1");
```

The Raylib backend implements this as `saves/<name>.json` files next to the executable on desktop, and browser
`localStorage` (via `EM_JS`) under a `game_save_` key prefix on WebAssembly.

