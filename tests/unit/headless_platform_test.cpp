#include <filesystem>
#include <vector>
#include <fstream>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

using engine::platform::HeadlessPlatform;
using engine::platform::PlatformRef;

namespace {

// An asset directory under the temp directory holding levels/beacon.json, removed when the test ends.
class AssetDirectory {
public:
	AssetDirectory() : root_(std::filesystem::temp_directory_path() / "headless_platform_test_assets") {
		std::filesystem::create_directories(root_ / "levels");
		std::ofstream(root_ / "levels" / "beacon.json")
			<< R"({"entities": [{"name": "Beacon", "components": {"transform": {"position": [1.0, 2.0, 3.0]}}}]})";
	}
	~AssetDirectory() { std::filesystem::remove_all(root_); }

	AssetDirectory(const AssetDirectory&) = delete;
	AssetDirectory& operator=(const AssetDirectory&) = delete;
	AssetDirectory(AssetDirectory&&) = delete;
	AssetDirectory& operator=(AssetDirectory&&) = delete;

	[[nodiscard]] std::string Path() const { return root_.generic_string(); }

private:
	std::filesystem::path root_;
};

TEST(HeadlessPlatform, AnAssetPathResolvesAgainstTheGivenDirectory) {
	const engine::EngineContext ctx{std::make_unique<HeadlessPlatform>("/srv/game/assets")};

	EXPECT_EQ(engine::assets::ResolveAsset(*ctx.GetWorld(), "models/room.glb"), "/srv/game/assets/models/room.glb");
}

TEST(HeadlessPlatform, AWorldLoadsALevelFileByAssetPath) {
	const AssetDirectory assets;
	engine::EngineContext ctx{std::make_unique<HeadlessPlatform>(assets.Path())};
	auto& world = *ctx.GetWorld();

	ASSERT_EQ(engine::level::LoadLevel(world, engine::assets::ResolveAsset(world, "levels/beacon.json")), 1);

	const auto beacon = world.lookup("Beacon");
	ASSERT_TRUE(beacon.is_valid());
	EXPECT_EQ(beacon.get<engine::spatial::Transform>().position, glm::vec3(1.0F, 2.0F, 3.0F));
}

TEST(HeadlessPlatform, AWorldCreatedWithoutAPlatformRunsOnAHeadlessOne) {
	const engine::EngineContext ctx;

	const auto* platform = ctx.GetWorld()->get<PlatformRef>().ptr;
	ASSERT_NE(platform, nullptr);
	EXPECT_FALSE(platform->HasWindow());
	EXPECT_EQ(platform->AssetDirectory(), ".");
}

// A save directory under the temp directory, empty at the start and removed when the test ends.
class TempSaves {
public:
	explicit TempSaves(const std::string& name) : root_(std::filesystem::temp_directory_path() / name) {
		std::filesystem::remove_all(root_);
	}
	~TempSaves() { std::filesystem::remove_all(root_); }

	TempSaves(const TempSaves&) = delete;
	TempSaves& operator=(const TempSaves&) = delete;
	TempSaves(TempSaves&&) = delete;
	TempSaves& operator=(TempSaves&&) = delete;

	[[nodiscard]] const std::filesystem::path& Path() const { return root_; }

private:
	std::filesystem::path root_;
};

TEST(HeadlessPlatform, ASaveWrittenIsReadBack) {
	const TempSaves dir{"headless_platform_test_saves_write"};
	HeadlessPlatform platform{".", dir.Path()};

	platform.WriteSave("world", R"({"day": 3})");

	EXPECT_TRUE(platform.HasSave("world"));
	EXPECT_EQ(platform.ReadSave("world"), R"({"day": 3})");
	EXPECT_TRUE(std::filesystem::is_regular_file(dir.Path() / "world.json"));
}

TEST(HeadlessPlatform, ASaveOutlivesThePlatformThatWroteIt) {
	const TempSaves dir{"headless_platform_test_saves_restart"};
	HeadlessPlatform{".", dir.Path()}.WriteSave("world", "v1");

	const HeadlessPlatform restarted{".", dir.Path()};

	EXPECT_EQ(restarted.ReadSave("world"), "v1");
}

TEST(HeadlessPlatform, AMissingSlotReadsAsEmpty) {
	const TempSaves dir{"headless_platform_test_saves_missing"};
	const HeadlessPlatform platform{".", dir.Path()};

	EXPECT_FALSE(platform.HasSave("nothing"));
	EXPECT_EQ(platform.ReadSave("nothing"), "");
	EXPECT_TRUE(platform.ListSaves().empty());
}

TEST(HeadlessPlatform, ADeletedSaveIsGone) {
	const TempSaves dir{"headless_platform_test_saves_delete"};
	HeadlessPlatform platform{".", dir.Path()};
	platform.WriteSave("world", "v1");

	platform.DeleteSave("world");

	EXPECT_FALSE(platform.HasSave("world"));
	EXPECT_EQ(platform.ReadSave("world"), "");
}

TEST(HeadlessPlatform, ListSavesReturnsSlotNamesSorted) {
	const TempSaves dir{"headless_platform_test_saves_list"};
	HeadlessPlatform platform{".", dir.Path()};
	platform.WriteSave("zeta", "1");
	platform.WriteSave("alpha", "2");
	platform.WriteSave("mid", "3");

	EXPECT_EQ(platform.ListSaves(), (std::vector<std::string>{"alpha", "mid", "zeta"}));
}

TEST(HeadlessPlatform, AnInvalidSlotNameWritesNothing) {
	const TempSaves dir{"headless_platform_test_saves_invalid"};
	HeadlessPlatform platform{".", dir.Path()};

	platform.WriteSave("../escape", "x");

	EXPECT_FALSE(platform.HasSave("../escape"));
	EXPECT_FALSE(std::filesystem::exists(dir.Path().parent_path() / "escape.json"));
}

TEST(HeadlessPlatform, ThePreviousSaveSurvivesAnInterruptedWrite) {
	const TempSaves dir{"headless_platform_test_saves_interrupted"};
	HeadlessPlatform platform{".", dir.Path()};
	platform.WriteSave("world", "v1");
	// A crash mid-write leaves the temporary file half written beside the slot.
	std::ofstream(dir.Path() / "world.json.tmp") << "{\"da";

	const HeadlessPlatform restarted{".", dir.Path()};

	EXPECT_EQ(restarted.ReadSave("world"), "v1");
	EXPECT_EQ(restarted.ListSaves(), std::vector<std::string>{"world"});
}

TEST(HeadlessPlatform, AWriteReplacesALeftoverTemporaryFile) {
	const TempSaves dir{"headless_platform_test_saves_leftover"};
	HeadlessPlatform platform{".", dir.Path()};
	platform.WriteSave("world", "v1");
	std::ofstream(dir.Path() / "world.json.tmp") << "{\"da";

	platform.WriteSave("world", "v2");

	EXPECT_EQ(platform.ReadSave("world"), "v2");
	EXPECT_FALSE(std::filesystem::exists(dir.Path() / "world.json.tmp"));
}

TEST(HeadlessPlatform, WithNoSavesWritesAreDropped) {
	HeadlessPlatform platform{".", HeadlessPlatform::kNoSaves};

	platform.WriteSave("world", "v1");

	EXPECT_FALSE(platform.SaveDirectory().has_value());
	EXPECT_FALSE(platform.HasSave("world"));
	EXPECT_EQ(platform.ReadSave("world"), "");
	EXPECT_TRUE(platform.ListSaves().empty());
}

TEST(HeadlessPlatform, SavesDefaultToTheDesktopLocation) {
	const HeadlessPlatform platform;

	EXPECT_EQ(platform.SaveDirectory(), engine::platform::DefaultSaveDirectory());
}

} // namespace
