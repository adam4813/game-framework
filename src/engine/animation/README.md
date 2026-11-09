# Animation

Kind-agnostic animation playback plus the skeletal mesh kind. Only the **skeletal** Animation Kind exists today; the
playback core (`AnimationPlayer`, `AdvanceAnimation`) knows nothing about meshes, so a sprite or spline kind can reuse
it later. Terms (Animation Kind, Animation Player) are in `docs/GLOSSARY.md`.

## Structure

- **animation_components.hpp** — `SkeletalAnimationSet`, `AnimationPlayer`, the `UnboundAnimationWarned` tag.
- **animation_playback.hpp** — `AdvanceAnimation(player, dt)`, the pure playback step the system and the GTest share.
- **animation_module.hpp/cpp** — reflection, scripting registration, level loaders, the asset resolver and systems.
- **animation.hpp** — public umbrella header.

Imported by `EngineContext` before the render module, whose `RenderSkinnedMeshes` draws the pose.

## Components

- `SkeletalAnimationSet { path, handle }` — the clips of a model file. A path asset (`AssetType::Animation`),
  deduplicated and reference-counted by path.
- `AnimationPlayer { clip, clip_index, clip_duration, time, speed, loop, playing }` — per-entity playback state.
  `clip` is a name (empty means the first clip). `clip_index == -1` means no kind has bound the player yet.
  `time` and `clip_duration` are seconds.

`SkeletalPose { clip, time }` is a value struct passed to `Platform::DrawSkinnedMesh`, not a component.

## Systems

- `BindSkeletalClips` (`OnUpdate`) — once the set's handle is valid, resolves `clip` to `clip_index` and
  `clip_duration`. An unknown name warns once and falls back to clip 0.
- `AdvanceAnimation` (`OnUpdate`, `Pausable`, `GameScene`) — `time += dt · max(speed, 0)`. Looping wraps with `fmod`;
  otherwise `time` clamps to `clip_duration` and `playing` becomes false. An unbound player warns once per entity and
  is skipped.

## Level JSON

```json
"mesh":                   { "path": "models/character.glb" },
"skeletal_animation_set": { "path": "models/character.glb" },
"animation":              { "clip": "", "speed": 1.0, "loop": true, "playing": true }
```

Every `animation` field is optional, so `"animation": {}` loops the first clip at normal speed. Two entities that
share a model play independently. Lit like everything else, point lights included.

## Backend notes

- A clip's length is `frameCount × 17 ms` (raylib resamples glTF clips to a fixed step), and the frame shown is
  `floor(time · 1000/17) mod frameCount`. Both stay inside the backend.
- Skinning is GPU only, through the skinned lit program (`lit_skinned.vs` + `lit.fs`). It supports **64 bones**
  (`MAX_BONES`); a model with more joints logs one error and draws in its bind pose.
- The bone matrices are set immediately before each entity's draw, so entities sharing a model can be at different
  frames in the same frame.
