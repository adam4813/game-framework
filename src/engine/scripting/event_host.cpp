#include "event_host.hpp"

#include "engine/ecs/world_logger.hpp"
#include "script_backend.hpp"
#include "scripting_module.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace engine::scripting {

namespace {

std::string_view KindName(const ScriptValueKind kind) {
	switch (kind) {
	case ScriptValueKind::Void: return "void";
	case ScriptValueKind::Bool: return "bool";
	case ScriptValueKind::Int: return "int";
	case ScriptValueKind::Float: return "float";
	case ScriptValueKind::String: return "string";
	case ScriptValueKind::Object: return "object";
	case ScriptValueKind::Array: return "array";
	}
	return "?";
}

// "void OnKill(string, int)"
std::string Signature(const ScriptFunctionInfo& function) {
	std::string text = std::string{KindName(function.return_kind)} + " " + function.name + "(";
	for (size_t i = 0; i < function.params.size(); ++i) {
		text += (i == 0 ? "" : ", ") + std::string{KindName(function.params[i])};
	}
	return text + ")";
}

} // namespace

HandlerReport
AnalyzeHandlers(const std::span<const ScriptEvent> events, const std::span<const ScriptFunctionInfo> functions) {
	HandlerReport report;
	for (const ScriptFunctionInfo& function : functions) {
		const auto match = std::ranges::find_if(events, [&function](const ScriptEvent& event) {
			return event.name == function.name && event.params == function.params;
		});
		if (match != events.end()) {
			report.defined.push_back(match->name);
		}
		else if (function.name.starts_with("On")) {
			report.unmatched.push_back(Signature(function));
		}
	}
	return report;
}

EventHost::EventHost(EventHostConfig config) : config_{std::move(config)}, scripts_(config_.slots.size()) {}

HandlerReport EventHost::Load(const flecs::world& world, const size_t slot, std::string id, IScriptInstance* instance) {
	Unload(world, slot);
	if (instance == nullptr) {
		return {};
	}
	const HandlerReport report = AnalyzeHandlers(config_.events, instance->Functions());
	auto& log = ecs::Log(world);
	for (const std::string& signature : report.unmatched) {
		log.warn(
			"{} {} script '{}': '{}' matches no event, so it never runs",
			config_.log_tag,
			config_.slots[slot],
			id,
			signature
		);
	}
	Script& script = scripts_[slot];
	script = {.id = std::move(id), .instance = instance, .handles = std::vector<bool>(config_.events.size())};
	for (const std::string& name : report.defined) {
		script.handles[static_cast<size_t>(FindEvent(name) - config_.events.data())] = true;
	}
	return report;
}

void EventHost::Unload(const flecs::world& world, const size_t slot) {
	Script& script = scripts_[slot];
	if (script.instance != nullptr) {
		if (const auto* backend = world.try_get<ScriptBackendSingleton>(); backend != nullptr && backend->backend) {
			backend->backend->DestroyInstance(script.instance);
		}
	}
	script = {};
	std::erase_if(timers_, [slot](const Timer& timer) { return timer.slot == slot; });
}

void EventHost::UnloadAll(const flecs::world& world) {
	for (size_t slot = 0; slot < scripts_.size(); ++slot) {
		Unload(world, slot);
	}
	queue_.clear();
	dispatching_ = kNotDispatching;
}

bool EventHost::Alive(const size_t slot) const { return scripts_[slot].instance != nullptr && !scripts_[slot].dead; }

std::vector<std::string> EventHost::AliveIds() const {
	std::vector<std::string> ids;
	for (size_t slot = 0; slot < scripts_.size(); ++slot) {
		if (Alive(slot)) {
			ids.push_back(scripts_[slot].id);
		}
	}
	return ids;
}

const ScriptEvent* EventHost::FindEvent(const std::string_view name) const {
	const auto found = std::ranges::find(config_.events, name, &ScriptEvent::name);
	return found == config_.events.end() ? nullptr : &*found;
}

void EventHost::Fire(const flecs::world& world, const std::string_view event, const std::span<const ScriptArg> args) {
	const ScriptEvent* spec = FindEvent(event);
	auto& log = ecs::Log(world);
	if (spec == nullptr) {
		log.warn("{} '{}' is not an event", config_.log_tag, event);
		return;
	}
	if (!std::ranges::equal(spec->params, args, {}, {}, [](const ScriptArg& arg) { return KindOf(arg); })) {
		log.error("{} {} fired with arguments that don't fit its parameters, ignored", config_.log_tag, event);
		return;
	}
	if (Dispatching()) {
		log.warn("{} {} fired from inside a handler, ignored", config_.log_tag, event);
		return;
	}
	const auto index = static_cast<size_t>(spec - config_.events.data());
	for (size_t slot = 0; slot < scripts_.size(); ++slot) {
		Dispatch(world, slot, index, args);
	}
}

void EventHost::Dispatch(
	const flecs::world& world,
	const size_t slot,
	const size_t event,
	const std::span<const ScriptArg> args
) {
	Script& script = scripts_[slot];
	if (!Alive(slot) || !script.handles[event]) {
		return;
	}
	dispatching_ = slot;
	std::string error;
	const ScriptCallResult result = script.instance->CallFunction(config_.events[event].name, args, &error);
	dispatching_ = kNotDispatching;
	if (result == ScriptCallResult::Failed) {
		script.dead = true;
		ecs::Log(world).error(
			"{} {} script '{}' disabled: {} failed: {}",
			config_.log_tag,
			config_.slots[slot],
			script.id,
			config_.events[event].name,
			error
		);
	}
}

bool EventHost::Queue(const flecs::world& world, std::string label, ApplyFn apply) {
	if (!Dispatching()) {
		ecs::Log(world).warn("{} {} called outside an event handler, ignored", config_.log_tag, label);
		return false;
	}
	queue_.push_back({.label = scripts_[dispatching_].id + ": " + label, .apply = std::move(apply)});
	return true;
}

bool EventHost::After(const flecs::world& world, const float seconds, std::string name) {
	std::string_view refusal;
	if (!Dispatching()) {
		refusal = "outside an event handler";
	}
	else if (config_.timer_event.empty()) {
		refusal = "this host has no timer event";
	}
	else if (seconds <= 0.0F) {
		refusal = "the delay isn't positive";
	}
	else if (static_cast<int>(timers_.size()) >= config_.max_timers) {
		refusal = "too many timers";
	}
	if (!refusal.empty()) {
		ecs::Log(world).warn("{} After({}, \"{}\") refused: {}", config_.log_tag, seconds, name, refusal);
		return false;
	}
	timers_.push_back({.left = seconds, .name = std::move(name), .slot = dispatching_});
	return true;
}

void EventHost::StepTimers(const flecs::world& world, const float dt) {
	const ScriptEvent* timer_event = FindEvent(config_.timer_event);
	std::vector<Timer> due;
	for (Timer& timer : timers_) {
		timer.left -= dt;
	}
	std::erase_if(timers_, [&due](const Timer& timer) {
		if (timer.left > 0.0F) {
			return false;
		}
		due.push_back(timer);
		return true;
	});
	if (timer_event == nullptr) {
		return;
	}
	const auto index = static_cast<size_t>(timer_event - config_.events.data());
	for (const Timer& timer : due) {
		const std::array<ScriptArg, 1> args{timer.name};
		Dispatch(world, timer.slot, index, args);
	}
}

std::vector<EventHost::AppliedCall> EventHost::ApplyQueue(const flecs::world& world) {
	std::vector<AppliedCall> results;
	for (int round = 0; round < config_.max_apply_rounds && !queue_.empty(); ++round) {
		const std::vector<QueuedCall> calls = std::exchange(queue_, {});
		for (const QueuedCall& call : calls) {
			std::string refusal = call.apply(world);
			if (!refusal.empty()) {
				ecs::Log(world).warn("{} {} refused: {}", config_.log_tag, call.label, refusal);
			}
			results.push_back({.label = call.label, .refusal = std::move(refusal)});
		}
	}
	return results;
}

} // namespace engine::scripting
