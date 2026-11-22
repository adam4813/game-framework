#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

namespace {

using engine::scripting::ScriptArg;
using engine::scripting::ScriptCallResult;
using engine::scripting::ScriptFunctionInfo;
using engine::scripting::ScriptValueKind;

// A script with state, handlers taking each argument kind, an overload, an output parameter, and one that throws.
constexpr auto kSource = R"(
int total = 0;
string last;
void OnAdd(int amount) { total += amount; }
void OnName(const string &in name, float scale) { last = name; }
void OnHit(int amount) { total += amount; }
void OnHit(const string &in name) { last = name; }
void OnCopy(string name) { last = name; }
void OnOut(int &out result) { result = 1; }
int Total() { return total; }
void OnExpectTen() { if (total != 10) { int[] none; none[3] = 1; } }
void OnExpectLast(const string &in name) { if (last != name) { int[] none; none[3] = 1; } }
void OnBoom() { int[] none; none[3] = 1; }
)";

struct CallWorld {
	std::unique_ptr<engine::EngineContext> ctx =
		std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());
	engine::scripting::IScriptInstance* instance{nullptr};

	CallWorld() {
		auto& backend = *ctx->GetWorld()->get_mut<engine::scripting::ScriptBackendSingleton>().backend;
		instance = backend.CreateInstanceFromSource("call_test", kSource, ctx->GetWorld()->entity());
	}

	~CallWorld() {
		if (instance != nullptr) {
			ctx->GetWorld()->get_mut<engine::scripting::ScriptBackendSingleton>().backend->DestroyInstance(instance);
		}
	}

	// Calls OnExpectLast, which throws unless the script's `last` is `name`.
	[[nodiscard]] ScriptCallResult ExpectLast(const std::string& name) const {
		const std::array<ScriptArg, 1> args{name};
		return instance->CallFunction("OnExpectLast", args);
	}
};

TEST(ScriptCallFunction, CallsADefinedFunctionWithItsArgumentsAndKeepsScriptState) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	const std::array<ScriptArg, 1> five{5};
	EXPECT_EQ(game.instance->CallFunction("OnAdd", five), ScriptCallResult::Ok);
	EXPECT_EQ(game.instance->CallFunction("OnAdd", five), ScriptCallResult::Ok);
	// The global `total` persisted across the two calls: OnExpectTen throws unless it reads 10.
	EXPECT_EQ(game.instance->CallFunction("OnExpectTen", {}), ScriptCallResult::Ok);
}

TEST(ScriptCallFunction, PassesStringAndFloatArguments) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	const std::array<ScriptArg, 2> args{std::string{"boss"}, 2.5F};
	EXPECT_EQ(game.instance->CallFunction("OnName", args), ScriptCallResult::Ok);
	EXPECT_EQ(game.ExpectLast("boss"), ScriptCallResult::Ok);
}

TEST(ScriptCallFunction, PassesAStringToAByValueParameter) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	const std::array<ScriptArg, 1> args{std::string{"copy"}};
	EXPECT_EQ(game.instance->CallFunction("OnCopy", args), ScriptCallResult::Ok);
	EXPECT_EQ(game.ExpectLast("copy"), ScriptCallResult::Ok);
}

TEST(ScriptCallFunction, CallsTheOverloadWhoseParametersMatchTheArguments) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	const std::array<ScriptArg, 1> name{std::string{"crab"}};
	const std::array<ScriptArg, 1> ten{10};
	EXPECT_EQ(game.instance->CallFunction("OnHit", name), ScriptCallResult::Ok);
	EXPECT_EQ(game.instance->CallFunction("OnHit", ten), ScriptCallResult::Ok);
	EXPECT_EQ(game.ExpectLast("crab"), ScriptCallResult::Ok);
	EXPECT_EQ(game.instance->CallFunction("OnExpectTen", {}), ScriptCallResult::Ok);
}

TEST(ScriptCallFunction, ReportsAFunctionWithNoMatchingParametersAsNotDefined) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	EXPECT_EQ(game.instance->CallFunction("OnNothing", {}), ScriptCallResult::NotDefined);
	// A defined name called with other arguments is not the same entry point.
	const std::array<ScriptArg, 1> half{0.5F};
	EXPECT_EQ(game.instance->CallFunction("OnAdd", half), ScriptCallResult::NotDefined);
	EXPECT_EQ(game.instance->CallFunction("OnAdd", {}), ScriptCallResult::NotDefined);
	// An output parameter can't take an argument.
	const std::array<ScriptArg, 1> one{1};
	EXPECT_EQ(game.instance->CallFunction("OnOut", one), ScriptCallResult::NotDefined);
}

TEST(ScriptCallFunction, ReportsAnExceptionWithItsMessageAndLine) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	std::string error;
	EXPECT_EQ(game.instance->CallFunction("OnBoom", {}, &error), ScriptCallResult::Failed);
	EXPECT_NE(error.find("line"), std::string::npos) << error;
}

TEST(ScriptCallFunction, ListsEachGlobalFunctionsNameReturnKindAndParameterKinds) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	const std::vector<ScriptFunctionInfo> functions = game.instance->Functions();
	const auto has = [&functions](const ScriptFunctionInfo& info) {
		return std::ranges::find(functions, info) != functions.end();
	};
	EXPECT_TRUE(has(
		{.name = "OnName",
		 .return_kind = ScriptValueKind::Void,
		 .params = {ScriptValueKind::String, ScriptValueKind::Float}}
	));
	EXPECT_TRUE(has({.name = "OnCopy", .return_kind = ScriptValueKind::Void, .params = {ScriptValueKind::String}}));
	EXPECT_TRUE(has({.name = "Total", .return_kind = ScriptValueKind::Int, .params = {}}));
	// A parameter no argument can fill (here an output reference) reports Object.
	EXPECT_TRUE(has({.name = "OnOut", .return_kind = ScriptValueKind::Void, .params = {ScriptValueKind::Object}}));
}

} // namespace
