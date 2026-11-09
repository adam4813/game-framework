#pragma once

#include <algorithm>
#include <cmath>

#include "animation_components.hpp"

namespace engine::animation {

/// Advances `player` by `dt` seconds. A player that isn't `playing` is left as is. Returns false, leaving it
/// untouched, when it should be advancing but is unbound (`clip_index == -1`). Time grows by
/// `dt * max(speed, 0)`: a negative speed counts as 0. With `loop` time wraps at `clip_duration`;
/// without it time clamps there and `playing` becomes false.
inline bool AdvanceAnimation(AnimationPlayer& player, const float dt) {
	if (!player.playing) {
		return true;
	}
	if (player.clip_index < 0) {
		return false;
	}
	player.time += dt * std::max(player.speed, 0.0F);
	if (player.time < player.clip_duration) {
		return true;
	}
	if (player.loop) {
		player.time = player.clip_duration > 0.0F ? std::fmod(player.time, player.clip_duration) : 0.0F;
	} else {
		player.time = player.clip_duration;
		player.playing = false;
	}
	return true;
}

} // namespace engine::animation
