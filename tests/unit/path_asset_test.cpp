#include <memory>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

using engine::render::AlbedoMap;

namespace {

constexpr int kFileTextureHandle = 3;
constexpr int kUploadedTextureHandle = 7;

// Loads every texture path as kFileTextureHandle and records each unload, so a test can see what the asset
// registry loaded and freed.
class TexturePlatform final : public engine::platform::HeadlessPlatform {
public:
	int LoadTexture(std::string_view) override { return kFileTextureHandle; }
	void UnloadTexture(const int handle) override { unloaded->push_back(handle); }

	std::shared_ptr<std::vector<int>> unloaded = std::make_shared<std::vector<int>>();
};

struct AssetWorld {
	std::shared_ptr<std::vector<int>> unloaded;
	std::unique_ptr<engine::EngineContext> ctx;

	AssetWorld() {
		auto platform = std::make_unique<TexturePlatform>();
		unloaded = platform->unloaded;
		ctx = std::make_unique<engine::EngineContext>(std::move(platform));
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }
};

TEST(PathAsset, AnAlbedoMapWithNoPathKeepsTheHandleItWasGiven) {
	const AssetWorld w;
	const auto e = w.World().entity().set<AlbedoMap>({.path = "", .handle = kUploadedTextureHandle});

	EXPECT_EQ(e.get<AlbedoMap>().handle, kUploadedTextureHandle);
}

TEST(PathAsset, AnAlbedoMapWithAPathLoadsThatTexture) {
	const AssetWorld w;
	const auto e = w.World().entity().set<AlbedoMap>({.path = "textures/checker.png"});

	EXPECT_EQ(e.get<AlbedoMap>().handle, kFileTextureHandle);
}

TEST(PathAsset, RemovingAnAlbedoMapWithNoPathLeavesItsTextureLoaded) {
	const AssetWorld w;
	const auto owner = w.World().entity().set<AlbedoMap>({.path = "textures/checker.png"});
	const auto borrower = w.World().entity().set<AlbedoMap>({.path = "", .handle = kFileTextureHandle});

	borrower.remove<AlbedoMap>();
	EXPECT_TRUE(w.unloaded->empty());

	owner.remove<AlbedoMap>();
	EXPECT_EQ(*w.unloaded, std::vector{kFileTextureHandle});
}

TEST(PathAsset, RemovingTheLastAlbedoMapWithAPathUnloadsItsTexture) {
	const AssetWorld w;
	const auto e = w.World().entity().set<AlbedoMap>({.path = "textures/checker.png"});

	e.remove<AlbedoMap>();

	EXPECT_EQ(*w.unloaded, std::vector{kFileTextureHandle});
}

} // namespace
