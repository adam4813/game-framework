#pragma once

namespace engine::timer {

enum class EasingType : int { Linear, EaseInQuad, EaseOutQuad, EaseInOutQuad };

// Countdown timer. A system decrements `remaining` each frame; when it reaches zero
// `expired` is set to true. If `repeat` is true, `remaining` is reset by `duration`.
struct Timer {
	float remaining{0.0F};
	float duration{0.0F};
	bool repeat{false};
	bool expired{false};
};

// Cooldown gate. A system decrements `remaining` each frame, clamping at zero.
// Use Ready() to test whether the cooldown has elapsed.
struct Cooldown {
	float remaining{0.0F};
	float duration{0.0F};
	[[nodiscard]] bool Ready() const { return remaining <= 0.0F; }
};

// Interpolates `value` from `from` to `to` over `duration` seconds using `easing`.
// A system advances `elapsed` and writes the current eased value into `value`.
// `done` is set to true once `elapsed >= duration`.
struct Tween {
	float elapsed{0.0F};
	float duration{1.0F};
	float from{0.0F};
	float to{1.0F};
	EasingType easing{EasingType::Linear};
	float value{0.0F};
	bool done{false};
};

// Optional marker tag added to an entity when its Timer expires.
struct TimerExpired {};

} // namespace engine::timer
