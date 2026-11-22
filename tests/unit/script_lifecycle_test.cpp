#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

namespace {

constexpr float kTickDt = 1.0F / 60.0F;

// What a test script records on its host.
struct ScriptProbe {
	int inits{0};
	int ticks{0};
	int destroyed{0};
	std::string text;
};

// The script throws when `ticks` reaches this, so a disabled script stops there.
constexpr int kThrowOnTick = 2;

// A world with a "Host" entity carrying a ScriptProbe and one script child running `source`.
struct ScriptWorld {
	std::unique_ptr<engine::EngineContext> ctx =
		std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());
	flecs::entity script;

	explicit ScriptWorld(const char* source) {
		flecs::world& world = World();
		world.component<ScriptProbe>()
			.member("inits", &ScriptProbe::inits)
			.member("ticks", &ScriptProbe::ticks)
			.member("destroyed", &ScriptProbe::destroyed)
			.member("text", &ScriptProbe::text);
		engine::scripting::RegisterComponentForScripts(world, world.component<ScriptProbe>());
		engine::scripting::RegisterGlobalConstantsForScripts(
			world,
			{{.name = "Quoted",
			  .type = engine::scripting::ScriptValueType::MakeString(),
			  .value_str = R"(say "hi" \o/)"}}
		);
		const auto host = world.entity("Host").set<ScriptProbe>({});
		script = world.entity().child_of(host);
		script.set<engine::scripting::ScriptComponent>({.inline_source = source});
	}

	void RunFrames(const int count) const {
		for (int i = 0; i < count; ++i) {
			engine::fixed_step::RunFrame(World(), kTickDt);
		}
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }
	[[nodiscard]] const ScriptProbe& Probe() const { return World().lookup("Host").get<ScriptProbe>(); }
	[[nodiscard]] const engine::scripting::ScriptComponent& Component() const {
		return script.get<engine::scripting::ScriptComponent>();
	}
};

// Counts ticks on its host (self) and throws on tick kThrowOnTick; OnDestroy marks the probe.
constexpr auto kThrowsInTick = R"(
	void Tick(Entity self, float dt) {
		ScriptProbe@ p = self.GetScriptProbe();
		p.ticks += 1;
		if (p.ticks == 2) { int[] none; none[3] = 1; }
	}
	void OnDestroy(Entity self) { self.GetScriptProbe().destroyed = 1; }
)";

TEST(ScriptLifecycle, ScriptSelfIsTheHostEntity) {
	const ScriptWorld w(R"(
		void OnInit(Entity self) { self.GetScriptProbe().text = self.GetName(); }
	)");
	w.RunFrames(1);
	EXPECT_EQ(w.Probe().text, "Host");
}

TEST(ScriptLifecycle, AScriptThatThrowsInTickIsDisabledAndNotCalledAgain) {
	const ScriptWorld w(kThrowsInTick);
	w.RunFrames(5);
	EXPECT_EQ(w.Probe().ticks, kThrowOnTick);
	EXPECT_EQ(w.Component().error, engine::scripting::ScriptError::RuntimeError);
	EXPECT_NE(w.Component().error_message.find("Tick"), std::string::npos) << w.Component().error_message;
}

TEST(ScriptLifecycle, ADisabledScriptSkipsOnDestroy) {
	const ScriptWorld w(kThrowsInTick);
	w.RunFrames(5);
	w.script.remove<engine::scripting::ScriptComponent>();
	EXPECT_EQ(w.Probe().destroyed, 0);
}

TEST(ScriptLifecycle, AHealthyScriptRunsOnDestroyWhenRemoved) {
	const ScriptWorld w(R"(
		void Tick(Entity self, float dt) { self.GetScriptProbe().ticks += 1; }
		void OnDestroy(Entity self) { self.GetScriptProbe().destroyed = 1; }
	)");
	w.RunFrames(1);
	w.script.remove<engine::scripting::ScriptComponent>();
	EXPECT_EQ(w.Probe().destroyed, 1);
}

TEST(ScriptLifecycle, AScriptThatThrowsInOnInitNeverTicks) {
	const ScriptWorld w(R"(
		void OnInit(Entity self) { self.GetScriptProbe().inits += 1; int[] none; none[3] = 1; }
		void Tick(Entity self, float dt) { self.GetScriptProbe().ticks += 1; }
	)");
	w.RunFrames(3);
	EXPECT_EQ(w.Probe().inits, 1);
	EXPECT_EQ(w.Probe().ticks, 0);
	EXPECT_EQ(w.Component().error, engine::scripting::ScriptError::RuntimeError);
}

TEST(ScriptLifecycle, AStringConstantKeepsItsQuotesAndBackslashes) {
	const ScriptWorld w(R"(
		void OnInit(Entity self) { self.GetScriptProbe().text = Quoted; }
	)");
	w.RunFrames(1);
	EXPECT_EQ(w.Component().error, engine::scripting::ScriptError::None) << w.Component().error_message;
	EXPECT_EQ(w.Probe().text, R"(say "hi" \o/)");
}

} // namespace
