#include <memory>

#include <gtest/gtest.h>

#include "engine/engine.hpp"
#include "null_platform.hpp"

using engine::input::InputState;
using engine::input::KeyCodeFromName;
using engine::input::KeyOverride;
namespace KeyCode = engine::input::KeyCode;

namespace {

TEST(KeyNames, KeyCodeConstantNamesResolveToTheirCodes) {
	EXPECT_EQ(KeyCodeFromName("W"), KeyCode::W);
	EXPECT_EQ(KeyCodeFromName("Up"), KeyCode::Up);
	EXPECT_EQ(KeyCodeFromName("LeftShift"), KeyCode::LeftShift);
	EXPECT_EQ(KeyCodeFromName("Key7"), KeyCode::Key7);
	EXPECT_EQ(KeyCodeFromName("F12"), KeyCode::F12);
	EXPECT_FALSE(KeyCodeFromName("w").has_value());
	EXPECT_FALSE(KeyCodeFromName("Shift").has_value());
}

// The keyless NullPlatform never reports a key, so anything down came from the override.
struct OverrideWorld {
	std::unique_ptr<engine::EngineContext> ctx =
		std::make_unique<engine::EngineContext>(std::make_unique<NullPlatform>());
	flecs::world& world = *ctx->GetWorld();

	// Read InputState mid-frame (as game logic does), after InputPoll and before the frame reset.
	InputState Frame() {
		InputState seen{};
		const auto probe = world.system("Probe").kind(flecs::OnUpdate).run([&](const flecs::iter& it) {
			seen = it.world().get<InputState>();
		});
		world.progress(1.0F / 60.0F);
		probe.destruct();
		return seen;
	}
};

TEST(KeyOverride, HeldKeysAreDownAndPressOnTheFirstFrameOnly) {
	OverrideWorld w;
	w.world.set<KeyOverride>({.held = "D LeftShift"});

	const auto first = w.Frame();
	const auto second = w.Frame();

	EXPECT_TRUE(first.IsKeyDown(KeyCode::D));
	EXPECT_TRUE(first.WasKeyPressed(KeyCode::LeftShift));
	EXPECT_TRUE(second.IsKeyDown(KeyCode::LeftShift));
	EXPECT_FALSE(second.WasKeyPressed(KeyCode::LeftShift));
	EXPECT_FALSE(first.IsKeyDown(KeyCode::A));
}

TEST(KeyOverride, ClearingTheOverrideReleasesTheKeys) {
	OverrideWorld w;
	w.world.set<KeyOverride>({.held = "D"});
	w.Frame();

	w.world.set<KeyOverride>({});
	const auto released = w.Frame();

	EXPECT_FALSE(released.IsKeyDown(KeyCode::D));
}

} // namespace
