---
name: flecs-remote-api
description: >
  Guide for verifying and controlling a running Flecs ECS game using the Flecs Remote API.
  Use this PROACTIVELY after implementing a new scene or gameplay loop — switch to the scene,
  exercise the core loop, confirm no runtime crashes before declaring the ticket done.
  Also use reactively for scene switching, UI navigation, input overrides, component queries,
  and ECS debugging when something is broken.
---

# Flecs Remote API Skill

The Flecs Remote API is integrated into the game and runs automatically. Use it to inspect and mutate the running ECS
state without modifying code.

## Setup

- **Port:** the engine default is `27750`; a Game Target may bake in its own (its `AGENTS.md` says which), and
  `FLECS_REST_PORT=<port>` on the game's launch overrides either. Set the same `FLECS_REST_PORT` for `flecs-api.js` and
  `flecs-mcp.js`, so you can run your instance beside the user's. After launch, `wait <Leaf>` confirms the server on
  that port is the game you started.
- **Explorer UI:** https://flecs.dev/explorer — auto-connects to `localhost:27750`
- **Script helper:** `tools/flecs-api.js` — Node.js wrapper (no dependencies) for querying and mutating the running
  game. Full usage is documented in the file header.
- **MCP tools:** `tools/flecs-mcp.js` exposes the same capabilities as `flecs_*` MCP tools (`flecs_switch_scene`,
  `flecs_list_scenes`, `flecs_pause`, `flecs_query`, etc.). When those tools are available in your session, prefer them
  over the CLI; otherwise use `tools/flecs-api.js`.

## Scene Switching

Each scene entity carries a stable `SceneId` identity pair (e.g., `(SceneId, Game)`). Switch by identity **name** — no
need to discover the runtime-assigned entity ID:

```bash
# List scenes with their identity name, runtime id, and active flag
node tools/flecs-api.js list-scenes

# Switch active scene by SceneId identity name (stable across launches)
node tools/flecs-api.js switch-scene Game
```

How it works:

- `switch-scene` resolves the identity via the scene's `(SceneId, <name>)` pair
- Removes the `Active` tag from the current scene and adds it to the target (mirrors `engine::scene::ActivateScene`)
- Waits until the target is actually active before returning; give the new scene's `Load()` a moment before querying the
  entities it creates
- A raw `#<id>` is still accepted if you need it

## Navigating UI Instead of Cheating

`switch-scene` forces the active scene directly. **Often it is better to drive the actual UI flow** — clicking the same
buttons a player would — so the scene's own transition logic, teardown, and side effects run exactly as shipped. This
exercises real code paths (e.g., a pause menu's "Quit to Title" `OnClick`) rather than bypassing them:

```bash
# List UI buttons (id, name, label), then fire one's OnClick by name or #id
node tools/flecs-api.js list-buttons
node tools/flecs-api.js click PlayTilemapButton   # or: click "#1006"
```

`click` fires the button's `OnClick` via a `UIClickRequest` (same path as a real click, including script-assigned
handlers). Prefer this UI navigation when validating end-to-end flows; fall back to `switch-scene` when you just need to
jump directly.

## Other Capabilities

- **Raw input override:** Use `override-input` / `restore-input` to drive input programmatically
- **Setting values:** `set <component> '<json>' [entity]` merges the JSON fields into a component and waits until a
  read-back shows them (`flecs_set` over MCP). Omit `entity` for a singleton, whose value lives on the component's own
  entity. A value a scene only reads in `Load` (e.g. a debug-overlay flag) takes effect after `switch-scene` to the
  already-active scene, which reloads it.
- **Querying:** Use the REST API endpoints to query entities, components, and system state

## Important Notes

- **Scene entity IDs are runtime-assigned.** Always discover them on each game launch via `list-scenes`; never hardcode
  them.
- **When MCP tools are available**, prefer `flecs_*` tools over shelling out to the CLI script.

## Reference

See [Flecs Remote API documentation](https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html) for full reference.
