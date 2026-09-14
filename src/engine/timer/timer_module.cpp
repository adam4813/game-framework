#include "timer_module.hpp"

#include <algorithm>
#include <spdlog/spdlog.h>
#include <string>

#include <flecs.h>
#include <nlohmann/json.hpp>

#include "engine/ecs/ecs.hpp"
#include "engine/level/level.hpp"
#include "engine/scripting/scripting.hpp"
#include "timer_components.hpp"

namespace {

float ApplyEasing(const engine::timer::EasingType type, const float t) {
	switch (type) {
	case engine::timer::EasingType::EaseInQuad: return t * t;
	case engine::timer::EasingType::EaseOutQuad: return t * (2.0F - t);
	case engine::timer::EasingType::EaseInOutQuad: return t < 0.5F ? 2.0F * t * t : -1.0F + (4.0F - 2.0F * t) * t;
	default: return t; // Linear
	}
}

} // namespace

namespace engine::timer {

TimerModule::TimerModule(const flecs::world& world) {
	// === Reflection ===
	world.component<Timer>()
		.member<float>("remaining")
		.member<float>("duration")
		.member<bool>("repeat")
		.member<bool>("expired");
	world.component<Cooldown>().member<float>("remaining").member<float>("duration");
	world.component<EasingType>();
	world.component<Tween>()
		.member<float>("elapsed")
		.member<float>("duration")
		.member<float>("from")
		.member<float>("to")
		.member<int>("easing") // EasingType backed by int; exposed as int to scripts
		.member<float>("value")
		.member<bool>("done");

	// === Scripting ===
	scripting::RegisterComponentForScripts(world, world.component<Timer>());
	scripting::RegisterComponentForScripts(world, world.component<Cooldown>());
	scripting::RegisterComponentMethodForScripts<&Cooldown::Ready>(world, "Ready");
	scripting::RegisterComponentForScripts(world, world.component<Tween>());

	// Expose EasingType enum values as integer constants so scripts can use readable names.
	scripting::RegisterGlobalConstantsForScripts(
		world,
		{
			{.name = "Easing_Linear",
			 .type = scripting::ScriptValueType::MakeInt(),
			 .value_str = std::to_string(static_cast<int>(EasingType::Linear))},
			{.name = "Easing_EaseInQuad",
			 .type = scripting::ScriptValueType::MakeInt(),
			 .value_str = std::to_string(static_cast<int>(EasingType::EaseInQuad))},
			{.name = "Easing_EaseOutQuad",
			 .type = scripting::ScriptValueType::MakeInt(),
			 .value_str = std::to_string(static_cast<int>(EasingType::EaseOutQuad))},
			{.name = "Easing_EaseInOutQuad",
			 .type = scripting::ScriptValueType::MakeInt(),
			 .value_str = std::to_string(static_cast<int>(EasingType::EaseInOutQuad))},
		}
	);

	// === Level loader ===
	level::RegisterComponentLoader(world, "timer", [](const flecs::entity e, const nlohmann::json& j) {
		Timer timer{};
		timer.remaining = j.value("remaining", timer.remaining);
		timer.duration = j.value("duration", timer.duration);
		timer.repeat = j.value("repeat", timer.repeat);
		timer.expired = j.value("expired", timer.expired);
		e.set<Timer>(timer);
	});
	level::RegisterComponentLoader(world, "cooldown", [](const flecs::entity e, const nlohmann::json& j) {
		Cooldown cooldown{};
		cooldown.remaining = j.value("remaining", cooldown.remaining);
		cooldown.duration = j.value("duration", cooldown.duration);
		e.set<Cooldown>(cooldown);
	});
	level::RegisterComponentLoader(world, "tween", [](const flecs::entity e, const nlohmann::json& j) {
		Tween tween{};
		tween.elapsed = j.value("elapsed", tween.elapsed);
		tween.duration = j.value("duration", tween.duration);
		tween.from = j.value("from", tween.from);
		tween.to = j.value("to", tween.to);
		tween.easing = j.value("easing", tween.easing);
		tween.value = j.value("value", tween.value);
		tween.done = j.value("done", tween.done);
		e.set<Tween>(tween);
	});

	// === SYSTEM: TimerAdvance ===
	std::ignore = world.system<Timer>("TimerAdvance")
					  .kind(flecs::OnUpdate)
					  .each([](const flecs::iter& it, size_t, Timer& t) {
						  if (t.remaining > 0.0F) {
							  t.remaining -= it.delta_time();
							  if (t.remaining <= 0.0F) {
								  t.expired = true;
								  if (t.repeat) {
									  t.remaining += t.duration;
								  }
							  }
						  }
					  })
					  .add<ecs::Pausable>();

	// === SYSTEM: CooldownAdvance ===
	std::ignore = world.system<Cooldown>("CooldownAdvance")
					  .kind(flecs::OnUpdate)
					  .each([](const flecs::iter& it, size_t, Cooldown& c) {
						  if (c.remaining > 0.0F) {
							  c.remaining = std::max(0.0F, c.remaining - it.delta_time());
						  }
					  })
					  .add<ecs::Pausable>();

	// === SYSTEM: TweenAdvance ===
	std::ignore = world.system<Tween>("TweenAdvance")
					  .kind(flecs::OnUpdate)
					  .each([](const flecs::iter& it, size_t, Tween& tw) {
						  if (tw.done) return;
						  tw.elapsed += it.delta_time();
						  const float normalised =
							  tw.duration > 0.0F ? std::clamp(tw.elapsed / tw.duration, 0.0F, 1.0F) : 1.0F;
						  const float eased = ApplyEasing(tw.easing, normalised);
						  tw.value = tw.from + (tw.to - tw.from) * eased;
						  tw.done = tw.elapsed >= tw.duration;
					  })
					  .add<ecs::Pausable>();

	spdlog::info("[TimerModule] Registered timer, cooldown and tween systems with Flecs");
}

} // namespace engine::timer
