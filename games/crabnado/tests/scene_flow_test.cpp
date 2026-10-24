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

	// Runs frames until `done` holds (or a time limit passes).
	template<typename Done>
	void RunUntil(Done done) const {
		for (int frame = 0; frame < 600 && !done(); ++frame) {
			World().progress(1.0F / 60.0F);
		}
	}

	// Clicks Play and runs the intro until the story comic's buttons show.
	void StartStory() const {
		Click("TitleUI::PlayButton");
		RunUntil([this] { return World().lookup(kSkipButton).get<ui::UIElement>().visible; });
	}

	// Clicks Play, skips the story comic, then runs frames until the arena loads.
	void Play() const {
		StartStory();
		Click(kSkipButton);
		RunUntil([this] { return IsActive<crabnado::ArenaSceneTag>(); });
	}

	static constexpr const char* kSkipButton = "TitleUI::StoryComic::SkipButton";
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

TEST(CrabnadoSceneFlow, SkippingTheStoryOpensTheGameScene) {
	const CrabnadoWorld game;

	game.Play();

	EXPECT_TRUE(game.IsActive<crabnado::ArenaSceneTag>());
	EXPECT_FALSE(game.IsActive<crabnado::TitleSceneTag>());
	EXPECT_FALSE(game.World().lookup("TitleUI").is_valid());
	EXPECT_TRUE(game.World().lookup("ArenaUI::PauseModal::QuitButton").is_valid());
}

TEST(CrabnadoSceneFlow, NextThroughEveryStoryPanelOpensTheGameScene) {
	const CrabnadoWorld game;
	game.StartStory();

	// One Next per panel still to show, then Fight! on the last.
	for (int click = 1; click < crabnado::StoryComic::kPanelCount; ++click) {
		game.Click("TitleUI::StoryComic::NextButton");
		game.World().progress(1.0F / 60.0F);
		EXPECT_TRUE(game.IsActive<crabnado::TitleSceneTag>());
	}
	EXPECT_EQ(game.World().lookup("TitleUI::StoryComic::NextButton").get<ui::Button>().label, "Fight!");
	game.Click("TitleUI::StoryComic::NextButton");
	game.RunUntil([&game] { return game.IsActive<crabnado::ArenaSceneTag>(); });

	EXPECT_TRUE(game.IsActive<crabnado::ArenaSceneTag>());
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
