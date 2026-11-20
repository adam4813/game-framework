#include <algorithm>
#include <array>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

namespace {

using engine::scripting::ScriptArg;
using engine::scripting::ScriptCallResult;

// A script with state, handlers taking each argument kind, and one that throws.
constexpr auto kSource = R"(
int total = 0;
string last;
void OnAdd(int amount) { total += amount; }
void OnName(const string &in name, float scale) { last = name; }
int Total() { return total; }
void OnExpectTen() { if (total != 10) { int[] none; none[3] = 1; } }
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
};

TEST(ScriptCallFunction, CallsADefinedFunctionWithItsArgumentsAndKeepsScriptState) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	const std::array<ScriptArg, 1> five{5};
	EXPECT_EQ(game.instance->CallFunction("void OnAdd(int)", five), ScriptCallResult::Ok);
	EXPECT_EQ(game.instance->CallFunction("void OnAdd(int)", five), ScriptCallResult::Ok);
	// The global `total` persisted across the two calls: OnExpectTen throws unless it reads 10.
	EXPECT_EQ(game.instance->CallFunction("void OnExpectTen()", {}), ScriptCallResult::Ok);
}

TEST(ScriptCallFunction, PassesStringAndFloatArguments) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	const std::array<ScriptArg, 2> args{std::string{"boss"}, 2.5F};
	EXPECT_EQ(game.instance->CallFunction("void OnName(const string &in, float)", args), ScriptCallResult::Ok);
}

TEST(ScriptCallFunction, ReportsAnUndefinedFunctionAsNotDefined) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	EXPECT_EQ(game.instance->CallFunction("void OnNothing()", {}), ScriptCallResult::NotDefined);
	// A defined name with another signature is not the same entry point.
	EXPECT_EQ(game.instance->CallFunction("void OnAdd(float)", {}), ScriptCallResult::NotDefined);
}

TEST(ScriptCallFunction, ReportsAnExceptionWithItsMessageAndLine) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	std::string error;
	EXPECT_EQ(game.instance->CallFunction("void OnBoom()", {}, &error), ScriptCallResult::Failed);
	EXPECT_NE(error.find("line"), std::string::npos) << error;
}

TEST(ScriptCallFunction, ListsTheGlobalFunctionDeclarationsWithoutParameterNames) {
	const CallWorld game;
	ASSERT_NE(game.instance, nullptr);
	const std::vector<std::string> declarations = game.instance->FunctionDeclarations();
	// AngelScript spells a reference parameter without a space: "const string&in".
	EXPECT_NE(std::ranges::find(declarations, "void OnName(const string&in, float)"), declarations.end());
	EXPECT_NE(std::ranges::find(declarations, "int Total()"), declarations.end());
}

} // namespace
