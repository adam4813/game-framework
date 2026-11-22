#pragma once

#include "script_method.hpp"

#include <flecs.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace engine::scripting {

class IScriptInstance;

// One event a host can fire: the handler's name and the kinds of the arguments it passes (Int, Float or String, the
// kinds a ScriptArg holds).
struct ScriptEvent {
	std::string name;
	std::vector<ScriptValueKind> params;
};

// Which events a script defines handlers for, and its On* functions that match no event (a typo or the wrong
// parameters, so the handler would never run), each written as "void OnKill(string)".
struct HandlerReport {
	std::vector<std::string> defined;
	std::vector<std::string> unmatched;
};

// Checks `functions` (IScriptInstance::Functions) against `events`. A handler matches an event with its name and
// parameter kinds, and returns void.
[[nodiscard]] HandlerReport
AnalyzeHandlers(std::span<const ScriptEvent> events, std::span<const ScriptFunctionInfo> functions);

struct EventHostConfig {
	// Starts every log line, e.g. "[Campaign]".
	std::string log_tag;
	std::vector<ScriptEvent> events;
	// The script slots, in dispatch order (e.g. "act", "stage"): an event runs each slot's handler in turn.
	std::vector<std::string> slots;
	// The event an After() timer fires, with the timer's name as its one String argument. Empty: no timers.
	std::string timer_event;
	int max_timers{64};
	// ApplyQueue goes round again for calls queued by handlers that applying fired, up to this many times.
	int max_apply_rounds{8};
};

// Runs scripts that react to discrete events a game fires (start, kill, timer), with none of them running per frame.
//
// Each slot holds one script. Fire runs the event's handler on each slot's script that defines it, in slot order. A
// game command a handler calls (a global function the game registers) goes through Queue, which stamps it with the
// script's id and holds it until the game calls ApplyQueue at a safe point; a command called outside a handler is
// logged and refused. After() arms a timer that fires `timer_event` on the script that set it. A handler that throws
// is logged once and disables its script, and only its script, until the slot is loaded again.
//
// The host destroys its scripts through the world's script backend: call UnloadAll before dropping it.
class EventHost {
public:
	// Applies one queued call; returns why it was refused, or an empty string.
	using ApplyFn = std::function<std::string(const flecs::world&)>;

	struct AppliedCall {
		std::string label; // "<script id>: <call>"
		std::string refusal;
	};

	EventHost() = default;
	explicit EventHost(EventHostConfig config);
	EventHost(EventHost&&) noexcept = default;
	EventHost& operator=(EventHost&&) noexcept = default;
	EventHost(const EventHost&) = delete;
	EventHost& operator=(const EventHost&) = delete;
	~EventHost() = default;

	// Puts `instance` (made by the world's backend) in `slot` as script `id`, destroying the script there before.
	// Logs each On* function that matches no event and returns the report. A null `instance` only empties the slot.
	HandlerReport Load(const flecs::world& world, size_t slot, std::string id, IScriptInstance* instance);
	// Destroys `slot`'s script and drops its timers.
	void Unload(const flecs::world& world, size_t slot);
	// Unloads every slot and drops the queued calls.
	void UnloadAll(const flecs::world& world);

	// Whether `slot` holds a script that hasn't been disabled.
	[[nodiscard]] bool Alive(size_t slot) const;
	// The ids of the scripts that are alive, in slot order.
	[[nodiscard]] std::vector<std::string> AliveIds() const;
	// The event named `name`, or null.
	[[nodiscard]] const ScriptEvent* FindEvent(std::string_view name) const;

	// Runs `event`'s handler on each slot that defines it. An unknown event, arguments that don't fit its parameters
	// and a Fire from inside a handler are logged and ignored.
	void Fire(const flecs::world& world, std::string_view event, std::span<const ScriptArg> args);

	// Queues a game command for the script whose handler is running. False (and logged) outside a handler.
	bool Queue(const flecs::world& world, std::string label, ApplyFn apply);
	// Arms a timer for the script whose handler is running. False (and logged) outside a handler, for a non-positive
	// delay, with no timer event or with max_timers already armed.
	bool After(const flecs::world& world, float seconds, std::string name);
	// Counts the timers down by `dt` and fires the ones that ran out.
	void StepTimers(const flecs::world& world, float dt);
	// Applies the queued calls in call order and logs each refusal. A call that fires an event can queue more, which
	// apply in the next round.
	std::vector<AppliedCall> ApplyQueue(const flecs::world& world);

private:
	struct Script {
		std::string id;
		IScriptInstance* instance{nullptr};
		std::vector<bool> defines_handler; // per event, looked up at load
		bool dead{false};
	};
	struct QueuedCall {
		std::string label;
		ApplyFn apply;
	};
	struct Timer {
		float remaining{0.0F};
		std::string name;
		size_t slot{0};
	};

	void Dispatch(const flecs::world& world, size_t slot, size_t event, std::span<const ScriptArg> args);
	[[nodiscard]] bool Dispatching() const { return dispatching_.has_value(); }
	[[nodiscard]] size_t EventIndex(const ScriptEvent& event) const {
		return static_cast<size_t>(&event - config_.events.data());
	}

	EventHostConfig config_;
	std::vector<Script> scripts_;
	std::vector<QueuedCall> queue_;
	std::vector<Timer> timers_;
	std::optional<size_t> dispatching_; // the slot whose handler is running
};

} // namespace engine::scripting
