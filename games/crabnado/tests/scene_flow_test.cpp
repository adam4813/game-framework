#include <memory>

#include <gtest/gtest.h>

#include "arena_scene.hpp"
#include "crabnado_module.hpp"
#include "engine/engine.hpp"
#include "null_platform.hpp"
#include "title_backdrop.hpp"
#include "title_scene.hpp"

using namespace engine;

namespace {

// The full engine stack on a headless platform, with Crabnado imported as the game does it.
struct CrabnadoWorld {
	std::unique_ptr<EngineContext> ctx =
		std::make_unique<EngineContext>(std::make_unique<NullPlatform>(CRABNADO_TEST_ASSET_DIR));

	CrabnadoWorld() { World().import<crabnado::CrabnadoModule>(); }

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	template<typename SceneTag>
	[[nodiscard]] bool IsActive() const {
		return scene::FindScene<SceneTag>(World()).template has<scene::Active>();
	}

	// Clicks a button the way the UI system does: inside a deferred frame, so the scene switch it
	// triggers (which destroys the button) lands after the callback returns.
	void Click(const char* path) const {
		const flecs::entity button = World().lookup(path);
		ASSERT_TRUE(button.is_valid()) << path;
		World().defer_begin();
		button.get<ui::OnClick>().callback(button);
		World().defer_end();
	}

	// Clicks Play, then runs frames until the intro it starts hands over to the arena (or a time limit passes).
	void Play() const {
		Click("TitleUI::PlayButton");
		for (int frame = 0; frame < 600 && !IsActive<crabnado::ArenaSceneTag>(); ++frame) {
			World().progress(1.0F / 60.0F);
		}
	}
};

TEST(CrabnadoSceneFlow, StartsOnTheTitleScene) {
	const CrabnadoWorld game;

	EXPECT_TRUE(game.IsActive<crabnado::TitleSceneTag>());
	EXPECT_FALSE(game.IsActive<crabnado::ArenaSceneTag>());
	EXPECT_TRUE(game.World().lookup("TitleUI::PlayButton").is_valid());
}

TEST(CrabnadoSceneFlow, PlayButtonStartsTheIntroOnTheTitleScene) {
	const CrabnadoWorld game;

	game.Click("TitleUI::PlayButton");
	game.World().progress(1.0F / 60.0F);

	EXPECT_TRUE(game.IsActive<crabnado::TitleSceneTag>());
	EXPECT_TRUE(game.World().lookup("TitleBackdrop::TitleCrab").has<crabnado::TitleScurry>());
}

TEST(CrabnadoSceneFlow, PlayButtonOpensTheGameSceneAfterTheIntro) {
	const CrabnadoWorld game;

	game.Play();

	EXPECT_TRUE(game.IsActive<crabnado::ArenaSceneTag>());
	EXPECT_FALSE(game.IsActive<crabnado::TitleSceneTag>());
	EXPECT_FALSE(game.World().lookup("TitleUI").is_valid());
	EXPECT_TRUE(game.World().lookup("ArenaUI::PauseModal::QuitButton").is_valid());
}

TEST(CrabnadoSceneFlow, PauseMenuQuitReturnsToTheTitleScene) {
	const CrabnadoWorld game;
	game.Play();

	game.Click("ArenaUI::PauseModal::QuitButton");

	EXPECT_TRUE(game.IsActive<crabnado::TitleSceneTag>());
	EXPECT_FALSE(game.World().lookup("ArenaUI").is_valid());
	EXPECT_TRUE(game.World().lookup("TitleUI::PlayButton").is_valid());
}

} // namespace
