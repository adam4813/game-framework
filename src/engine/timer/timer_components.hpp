#pragma once

namespace engine::timer {

enum class EasingType : int { Linear, EaseInQuad, EaseOutQuad, EaseInOutQuad };

struct Timer {
	float remaining{0.0F};
	float duration{0.0F};
	bool repeat{false};
	bool expired{false};
};

struct Cooldown {
	float remaining{0.0F};
	float duration{0.0F};
	[[nodiscard]] bool Ready() const { return remaining <= 0.0F; }
};

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
