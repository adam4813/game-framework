#include <filesystem>
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

} // namespace
