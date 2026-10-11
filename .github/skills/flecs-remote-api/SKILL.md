---
name: flecs-remote-api
description: Guide for inspecting and controlling a running Flecs ECS game using the Flecs Remote API, including scene switching, UI navigation, input overrides, and component queries. Use this when you need to inspect game state, navigate scenes, test UI flows, or debug the ECS runtime without modifying code.
---

# Flecs Remote API Skill

The Flecs Remote API is integrated into the game and runs automatically. Use it to inspect and mutate the running ECS
state without modifying code.

## Setup

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
- **Component queries:** Set component values via the REST API: `PUT /component/<path>?component=<c>&value=<json>` —
  e.g., to place an entity on a specific tile
- **Querying:** Use the REST API endpoints to query entities, components, and system state

## Important Notes

- **Scene entity IDs are runtime-assigned.** Always discover them on each game launch via `list-scenes`; never hardcode
  them.
- **When MCP tools are available**, prefer `flecs_*` tools over shelling out to the CLI script.

## Reference

See [Flecs Remote API documentation](https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html) for full reference.
