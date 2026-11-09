#pragma once

#include <string>

namespace engine::animation {

// The clips of one model file, loaded as a path asset (AssetType::Animation, deduplicated and
// reference-counted by path). Pair it with MeshPrimitive on the same entity to draw the skinned model.
// This is the skeletal Animation Kind; `handle` is -1 until the asset resolves.
struct SkeletalAnimationSet {
	std::string path;
	int handle{-1};
};

// Playback state for one animated entity. Kind-agnostic: it knows nothing of meshes, so a sprite or spline
// kind can reuse it. `clip` is a name (empty means the first clip); a kind's binding system resolves it to
// `clip_index` and `clip_duration`. `time` and `clip_duration` are seconds. `clip_index == -1` means no kind
// has bound the player yet.
struct AnimationPlayer {
	std::string clip;
	int clip_index{-1};
	float clip_duration{0.0F};
	float time{0.0F};
	float speed{1.0F};
	bool loop{true};
	bool playing{true};
};

// Marks a player that AdvanceAnimation already warned is unbound, so the warning fires once per entity.
struct UnboundAnimationWarned {};

} // namespace engine::animation
