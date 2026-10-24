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
- ~~Reword the rule~~ Done 2026-10-23 (Crabnado ticket 01 retro): `CODING_STANDARDS.md`, `docs/README.md`, the
  `create-module` skill and `src/engine/debug/README.md` now scope systems by `.child_of(sceneRoot_)` or a mode tag,
  and say the identity tag belongs on the scene entity only.
- Add a unit test: a system carrying a scene's identity tag doesn't stop `ActivateScene` from activating that scene.
