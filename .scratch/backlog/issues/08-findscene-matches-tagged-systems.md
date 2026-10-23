# Follow-up: FindScene matches any entity carrying the scene tag

Status: needs-triage
Source: retro of Signal Room ticket 01 (game branch `feat/signal-room`), 2026-10-18

## Context

`scene::FindScene<Tag>` (`src/engine/scene/scene_module.hpp`) returns the first entity that has `Tag`.
`ActivateScene` and `ReloadScene` then add or remove `Active` on that entity. Root `AGENTS.md` (ECS Conventions)
says "Scene-specific systems use `.add<scene::YourSceneTag>()` on the returned entity for pipeline filtering". If a
system follows that rule with the scene's identity tag, `FindScene` can return the system instead of the scene. The
scene then never activates, and `Active` lands on a system.

The sandbox avoids this in practice: its systems use the mode tags (`scene::GameScene`, `scene::MenuScene`) or are
parented to a scene root with `child_of`. Signal Room's Room panel is parented to the Room's root for this reason. Both
code reviewers on ticket 01 flagged that as a breach of the `AGENTS.md` rule.

## What to do

- Make `FindScene` match only scene entities: `.with<SceneTag>().with<SceneComponent>()`.
- Reword the `AGENTS.md` rule: scope scene systems by parenting them to the scene root (`.child_of(sceneRoot_)`), or
  filter them with a mode tag (`scene::GameScene` / `scene::MenuScene`). The identity tag belongs on the scene entity
  only.
- Add a unit test: a system carrying a scene's identity tag doesn't stop `ActivateScene` from activating that scene.
