# Timer Module

Lightweight Flecs module providing per-entity countdown timers, cooldowns, and tweens.

## Structure

- **timer_components.hpp** — Plain-data components (`Timer`, `Cooldown`, `Tween`), `EasingType` enum, and the
  `TimerExpired` marker tag.
- **timer_module.hpp** — `TimerModule` class declaration (Flecs module entry point).
- **timer_module.cpp** — Constructor: reflects components for the Flecs Explorer, registers them with the scripting
  backend, and registers the three advance systems.
- **timer.hpp** — Umbrella header; include this from game code.

## Usage

```cpp
// Import the module once during world setup
world.import<engine::timer::TimerModule>();

// Attach a one-shot timer (fires after 2 s)
entity.set<engine::timer::Timer>({ .remaining = 2.0F, .duration = 2.0F });

// Attach a repeating timer (fires every 0.5 s)
entity.set<engine::timer::Timer>({ .remaining = 0.5F, .duration = 0.5F, .repeat = true });

// Check expiry in a system or observer
if (t.expired) { /* react */ t.expired = false; }

// Attach a cooldown (ability ready check)
entity.set<engine::timer::Cooldown>({ .remaining = 1.0F, .duration = 1.0F });
if (cooldown.Ready()) { /* fire ability, then reset */ cooldown.remaining = cooldown.duration; }

// Attach a tween (ease a value from 0 to 1 over 0.3 s)
entity.set<engine::timer::Tween>({
    .duration = 0.3F,
    .from     = 0.0F,
    .to       = 1.0F,
    .easing   = engine::timer::EasingType::EaseOutQuad,
});
// Read tween.value each frame; tween.done is true when elapsed >= duration.
```

## Registered Systems

All three systems use `.each()`, run in `flecs::OnUpdate`, carry `.add<engine::ecs::Pausable>()`
(halt while the game is paused), and are **untagged** (run in every scene pipeline).

| System            | Component  | Behaviour                                                                  |
|-------------------|------------|----------------------------------------------------------------------------|
| `TimerAdvance`    | `Timer`    | Decrements `remaining`; sets `expired = true` at zero; resets if `repeat`. |
| `CooldownAdvance` | `Cooldown` | Decrements `remaining`, clamped to zero.                                   |
| `TweenAdvance`    | `Tween`    | Advances `elapsed`, applies easing, writes `value`; sets `done` at end.    |
