#include <array>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

namespace {

using engine::scripting::AnalyzeHandlers;
using engine::scripting::EventHost;
using engine::scripting::HandlerReport;
using engine::scripting::ScriptArg;
using engine::scripting::ScriptEvent;
using engine::scripting::ScriptFunctionInfo;
using enum engine::scripting::ScriptValueKind;

const std::vector<ScriptEvent> kEvents{
	{.name = "OnStart", .params = {}},
	{.name = "OnKill", .params = {String, String}},
	{.name = "OnTimer", .params = {String}},
};

TEST(ScriptEventHandlers, AScriptsHandlersAreCheckedAgainstTheEventTable) {
	const std::vector<ScriptFunctionInfo> functions{
		{.name = "OnKill", .return_kind = Void, .params = {String, String}},
		{.name = "OnTimer", .return_kind = Void, .params = {String}},
		{.name = "OnKill", .return_kind = Void, .params = {String}},
		{.name = "Helper", .return_kind = Void, .params = {Int}},
		{.name = "OnStrat", .return_kind = Void, .params = {}},
		{.name = "OnStart", .return_kind = Int, .params = {}},
	};
	const HandlerReport report = AnalyzeHandlers(kEvents, functions);
	EXPECT_EQ(report.defined, (std::vector<std::string>{"OnKill", "OnTimer"}));
	// A handler with the wrong parameters, or an On* name that is no event, is reported; a plain helper is not.
	EXPECT_EQ(report.unmatched, (std::vector<std::string>{"void OnKill(string)", "void OnStrat()", "int OnStart()"}));
}

// The test's host: an EventHost with two slots, "first" and "second", and a game command Note(text) that queues a call
// appending `text` to `applied` (an empty text is refused).
struct HostState {
	EventHost host{{.log_tag = "[Test]", .events = kEvents, .slots = {"first", "second"}, .timer_event = "OnTimer"}};
	std::vector<std::string> applied;
};

struct HostWorld {
	std::unique_ptr<engine::EngineContext> ctx =
		std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());

	HostWorld() {
		using namespace engine::scripting;
		World().component<HostState>();
		World().set<HostState>({});
		const auto text = [](const char* name) { return ScriptMethodParam{ScriptValueType::MakeString(), true, name}; };
		RegisterGlobalFunctionForScripts(
			World(),
			{.name = "Note", .return_type = ScriptValueType::MakeVoid(), .params = {text("text")}},
			[](ScriptCallContext& call, flecs::world& w) {
				const std::string note = call.GetArgString(0);
				std::ignore =
					w.get_mut<HostState>().host.Queue(w, "Note(" + note + ")", [note](const flecs::world& world) {
						if (note.empty()) {
							return std::string{"empty"};
						}
						world.get_mut<HostState>().applied.push_back(note);
						return std::string{};
					});
			}
		);
		RegisterGlobalFunctionForScripts(
			World(),
			{.name = "After",
			 .return_type = ScriptValueType::MakeVoid(),
			 .params = {{ScriptValueType::MakeFloat(), false, "seconds"}, text("name")}},
			[](ScriptCallContext& call, flecs::world& w) {
				std::ignore = w.get_mut<HostState>().host.After(w, call.GetArgFloat(0), call.GetArgString(1));
			}
		);
	}

	~HostWorld() { Host().UnloadAll(World()); }

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }
	[[nodiscard]] EventHost& Host() const { return World().get_mut<HostState>().host; }
	[[nodiscard]] const std::vector<std::string>& Applied() const { return World().get<HostState>().applied; }

	[[nodiscard]] engine::scripting::IScriptInstance* Compile(const std::string& id, const char* source) const {
		auto& backend = *World().get_mut<engine::scripting::ScriptBackendSingleton>().backend;
		return backend.CreateInstanceFromSource(id, source, World().entity());
	}

	HandlerReport Load(const size_t slot, const std::string& id, const char* source) const {
		return Host().Load(World(), slot, id, Compile(id, source));
	}
};

TEST(ScriptEventHost, TwoScriptsOnOneEventTableEachRunTheirHandlerInSlotOrder) {
	const HostWorld game;
	game.Load(1, "b", R"(void OnStart() { Note("second"); })");
	game.Load(0, "a", R"(void OnStart() { Note("first"); })");
	game.Host().Fire(game.World(), "OnStart", {});
	// A handler's call waits in the queue until the host applies it.
	EXPECT_TRUE(game.Applied().empty());
	const auto results = game.Host().ApplyQueue(game.World());
	EXPECT_EQ(game.Applied(), (std::vector<std::string>{"first", "second"}));
	ASSERT_EQ(results.size(), 2U);
	EXPECT_EQ(results[0].label, "a: Note(first)");
	EXPECT_TRUE(results[0].refusal.empty());
}

TEST(ScriptEventHost, AnEventPassesItsArgumentsAndASlotWithoutTheHandlerIsSkipped) {
	const HostWorld game;
	game.Load(0, "a", R"(void OnKill(const string &in type, const string &in source) { Note(type + "@" + source); })");
	game.Load(1, "b", R"(void OnStart() { Note("never"); })");
	const std::array<ScriptArg, 2> args{std::string{"crab"}, std::string{"north"}};
	game.Host().Fire(game.World(), "OnKill", args);
	std::ignore = game.Host().ApplyQueue(game.World());
	EXPECT_EQ(game.Applied(), (std::vector<std::string>{"crab@north"}));
}

TEST(ScriptEventHost, AMisspelledHandlerIsReportedAtLoad) {
	const HostWorld game;
	const HandlerReport report = game.Load(0, "a", R"(void OnStrat() { Note("typo"); } void OnStart() {})");
	EXPECT_EQ(report.defined, (std::vector<std::string>{"OnStart"}));
	EXPECT_EQ(report.unmatched, (std::vector<std::string>{"void OnStrat()"}));
}

TEST(ScriptEventHost, ACallMadeOutsideAHandlerIsRefused) {
	const HostWorld game;
	engine::scripting::IScriptInstance* instance = game.Compile("a", R"(void Poke() { Note("outside"); })");
	game.Host().Load(game.World(), 0, "a", instance);
	// Poke is no event, so only a direct call runs it, with no handler dispatching.
	EXPECT_EQ(instance->CallFunction("Poke", {}), engine::scripting::ScriptCallResult::Ok);
	EXPECT_TRUE(game.Host().ApplyQueue(game.World()).empty());
	EXPECT_TRUE(game.Applied().empty());
	EXPECT_FALSE(game.Host().Queue(game.World(), "Note(direct)", [](const flecs::world&) { return std::string{}; }));
	EXPECT_FALSE(game.Host().After(game.World(), 1.0F, "direct"));
}

TEST(ScriptEventHost, ARefusedCallIsReportedWithItsReason) {
	const HostWorld game;
	game.Load(0, "a", R"(void OnStart() { Note(""); })");
	game.Host().Fire(game.World(), "OnStart", {});
	const auto results = game.Host().ApplyQueue(game.World());
	ASSERT_EQ(results.size(), 1U);
	EXPECT_EQ(results[0].refusal, "empty");
}

TEST(ScriptEventHost, AThrowingHandlerDisablesOnlyItsOwnScript) {
	const HostWorld game;
	game.Load(0, "a", R"(void OnStart() { int[] none; none[3] = 1; })");
	game.Load(1, "b", R"(void OnStart() { Note("second"); })");
	game.Host().Fire(game.World(), "OnStart", {});
	game.Host().Fire(game.World(), "OnStart", {});
	std::ignore = game.Host().ApplyQueue(game.World());
	EXPECT_FALSE(game.Host().Alive(0));
	EXPECT_TRUE(game.Host().Alive(1));
	EXPECT_EQ(game.Host().AliveIds(), (std::vector<std::string>{"b"}));
	EXPECT_EQ(game.Applied(), (std::vector<std::string>{"second", "second"}));
}

TEST(ScriptEventHost, AnAfterTimerFiresTheTimerEventOnTheScriptThatSetItOnly) {
	const HostWorld game;
	game.Load(0, "a", R"(
void OnStart() { After(1.0f, "ping"); }
void OnTimer(const string &in name) { Note("a:" + name); })");
	game.Load(1, "b", R"(void OnTimer(const string &in name) { Note("b:" + name); })");
	game.Host().Fire(game.World(), "OnStart", {});
	game.Host().StepTimers(game.World(), 0.5F);
	std::ignore = game.Host().ApplyQueue(game.World());
	EXPECT_TRUE(game.Applied().empty());
	game.Host().StepTimers(game.World(), 0.6F);
	std::ignore = game.Host().ApplyQueue(game.World());
	EXPECT_EQ(game.Applied(), (std::vector<std::string>{"a:ping"}));
}

TEST(ScriptEventHost, UnloadingAScriptDropsItsTimers) {
	const HostWorld game;
	game.Load(0, "a", R"(
void OnStart() { After(1.0f, "ping"); }
void OnTimer(const string &in name) { Note(name); })");
	game.Host().Fire(game.World(), "OnStart", {});
	game.Load(0, "a2", R"(void OnTimer(const string &in name) { Note("replaced:" + name); })");
	game.Host().StepTimers(game.World(), 2.0F);
	std::ignore = game.Host().ApplyQueue(game.World());
	EXPECT_TRUE(game.Applied().empty());
}

} // namespace
