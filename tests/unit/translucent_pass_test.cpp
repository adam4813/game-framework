#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "engine/engine.hpp"

using engine::platform::BatchStats;
using engine::platform::DrawMaterial;
using engine::platform::RenderCommand;
using engine::render::SortBackToFront;
using engine::render::TranslucentDraw;

namespace {

// ── Back-to-front sort ──────────────────────────────────────────────────────────────────────────

TranslucentDraw At(const glm::vec3 center, const int handle) { return {.center = center, .handle = handle}; }

std::vector<int> Handles(const std::vector<TranslucentDraw>& draws) {
	std::vector<int> handles;
	for (const auto& d : draws) {
		handles.push_back(d.handle);
	}
	return handles;
}

TEST(TranslucentSort, FarthestFromTheEyeDrawsFirst) {
	std::vector draws{At({0, 0, -2}, 2), At({0, 0, -8}, 8), At({0, 0, -5}, 5)};
	SortBackToFront(draws, {0, 0, 0});
	EXPECT_EQ(Handles(draws), (std::vector{8, 5, 2}));
}

TEST(TranslucentSort, DistanceIsMeasuredInEveryAxisFromTheEye) {
	// Eye at (10, 0, 0): the draw at the origin is 10 away, the one at (10, 0, -3) only 3.
	std::vector draws{At({10, 0, -3}, 1), At({0, 0, 0}, 2)};
	SortBackToFront(draws, {10, 0, 0});
	EXPECT_EQ(Handles(draws), (std::vector{2, 1}));
}

TEST(TranslucentSort, EqualDistancesKeepTheirCollectionOrder) {
	std::vector draws{At({3, 0, 0}, 1), At({-3, 0, 0}, 2), At({0, 3, 0}, 3)};
	SortBackToFront(draws, {0, 0, 0});
	EXPECT_EQ(Handles(draws), (std::vector{1, 2, 3}));
}

// ── The render passes ───────────────────────────────────────────────────────────────────────────

// One draw the platform saw: which call, the world z of its transform, and whether it wrote depth.
struct DrawCall {
	std::string_view call;
	int handle{-1};
	float z{0.0F};
	bool depth_write{true};
	uint8_t alpha{255};
};

// A headless platform that records the 3D draws the render systems issue, in order.
class RecordingPlatform : public engine::platform::HeadlessPlatform {
public:
	std::vector<DrawCall> calls;

	void DrawCube(const glm::mat4& transform, glm::vec3, const DrawMaterial& m) override {
		calls.push_back({.call = "cube", .z = transform[3].z, .depth_write = m.depth_write, .alpha = m.tint.a});
	}
	void DrawMesh(const int handle, const glm::mat4& transform, const DrawMaterial& m) override {
		calls.push_back(
			{.call = "mesh", .handle = handle, .z = transform[3].z, .depth_write = m.depth_write, .alpha = m.tint.a}
		);
	}
	void DrawLine3D(const glm::vec3 from, glm::vec3, const engine::core::Rgba color) override {
		calls.push_back({.call = "line", .z = from.z, .alpha = color.a});
	}
	BatchStats SubmitBatch(const std::span<const RenderCommand> commands) override {
		for (const auto& c : commands) {
			calls.push_back(
				{.call = "batch",
				 .z = c.transform[3].z,
				 .depth_write = c.material.depth_write,
				 .alpha = c.material.tint.a}
			);
		}
		return {};
	}
};

constexpr engine::core::Rgba kOpaque{.r = 200, .g = 200, .b = 200, .a = 255};
constexpr engine::core::Rgba kGlass{.r = 120, .g = 200, .b = 255, .a = 100};

// A world on the RecordingPlatform with a camera at z = 10 looking down -Z, and the render systems run by hand in
// their registration order (the scene pipeline isn't needed to exercise them).
struct RenderWorld {
	std::unique_ptr<engine::EngineContext> ctx;
	RecordingPlatform* platform{nullptr};

	RenderWorld() {
		auto recording = std::make_unique<RecordingPlatform>();
		platform = recording.get();
		ctx = std::make_unique<engine::EngineContext>(std::move(recording));
		World().entity("Camera").set<engine::spatial::Transform>({.position = {0, 0, 10}}).set<engine::render::Camera>({});
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	flecs::entity Cube(const char* name, const float z, const engine::core::Rgba color) const {
		return World()
			.entity(name)
			.set<engine::spatial::Transform>({.position = {0, 0, z}})
			.set<engine::render::CubePrimitive>({})
			.set<engine::render::Material>({.color = color});
	}

	// An empty path keeps the given handle, as a runtime-loaded model would.
	flecs::entity Model(const char* name, const int handle, const float z, const engine::core::Rgba color) const {
		return World()
			.entity(name)
			.set<engine::spatial::Transform>({.position = {0, 0, z}})
			.set<engine::render::MeshPrimitive>({.path = "", .handle = handle})
			.set<engine::render::Material>({.color = color});
	}

	void RunSystem(const std::string_view name) const {
		flecs::entity found;
		World().query_builder<>().with(flecs::System).build().each([&](const flecs::entity e) {
			if (e.name() && std::string_view{e.name().c_str()} == name) {
				found = e;
			}
		});
		ASSERT_TRUE(found) << "no system named " << name;
		World().system(found).run();
	}

	void RenderFrame() const {
		for (const auto* name :
			 {"Render3DBegin", "RenderCubes", "RenderMeshes", "Render3DFlush", "Render3DTranslucent", "Render3DEnd"}) {
			RunSystem(name);
		}
	}
};

// Two opaque objects at the back and three translucent ones in front, created nearest first so the pass must reorder.
struct MixedScene : RenderWorld {
	MixedScene() {
		Cube("OpaqueCube", -10.0F, kOpaque);
		Model("OpaqueModel", 1, -10.0F, kOpaque);
		Model("GlassNear", 2, 0.0F, kGlass).add<engine::render::Translucent>();
		Model("GlassFar", 3, -5.0F, kGlass).add<engine::render::Translucent>();
		Cube("GlassCube", -2.0F, kGlass).add<engine::render::Translucent>();
	}
};

TEST(TranslucentPass, TranslucentEntitiesSkipTheOpaquePasses) {
	const MixedScene scene;
	scene.RenderFrame();

	std::vector<DrawCall> opaque;
	for (const auto& c : scene.platform->calls) {
		if (c.depth_write) opaque.push_back(c);
	}
	ASSERT_EQ(opaque.size(), 2U);
	EXPECT_EQ(opaque[0].call, "mesh");
	EXPECT_EQ(opaque[0].handle, 1);
	EXPECT_EQ(opaque[1].call, "batch");
	EXPECT_FLOAT_EQ(opaque[1].z, -10.0F);
}

TEST(TranslucentPass, TranslucentEntitiesDrawAfterAllOpaqueGeometryFarthestFirst) {
	const MixedScene scene;
	scene.RenderFrame();

	const auto& calls = scene.platform->calls;
	ASSERT_EQ(calls.size(), 5U);
	// Opaque first: the direct model draw, then the instanced batch.
	EXPECT_TRUE(calls[0].depth_write);
	EXPECT_TRUE(calls[1].depth_write);
	// Then the translucent pass, back to front from the camera at z = 10.
	EXPECT_EQ(calls[2].call, "mesh");
	EXPECT_EQ(calls[2].handle, 3); // z = -5, 15 away
	EXPECT_EQ(calls[3].call, "cube");
	EXPECT_FLOAT_EQ(calls[3].z, -2.0F); // 12 away
	EXPECT_EQ(calls[4].call, "mesh");
	EXPECT_EQ(calls[4].handle, 2); // z = 0, 10 away
}

TEST(TranslucentPass, TranslucentDrawsKeepTheirAlphaAndWriteNoDepth) {
	const MixedScene scene;
	scene.RenderFrame();

	for (const auto& c : scene.platform->calls) {
		if (c.alpha == kGlass.a) {
			EXPECT_FALSE(c.depth_write) << c.call << " at z " << c.z;
		}
	}
}

TEST(TranslucentPass, RemovingTheTagReturnsTheEntityToTheOpaquePass) {
	const MixedScene scene;
	scene.World().lookup("GlassCube").remove<engine::render::Translucent>();
	scene.RenderFrame();

	int batched = 0;
	for (const auto& c : scene.platform->calls) {
		if (c.call == "batch") ++batched;
		EXPECT_NE(c.call, "cube") << "the cube left the translucent pass";
	}
	EXPECT_EQ(batched, 2);
}

// Through the real pipeline, not RenderFrame's list: lines write depth like opaque geometry, so they must draw before
// the translucent pass, or a translucent object in front of a line couldn't blend over it.
TEST(TranslucentPass, QueuedLinesDrawBeforeTheTranslucentPass) {
	const MixedScene scene;
	scene.World().get_mut<engine::render::LineQueue>().lines.push_back({.from = {-1, 0, -8}, .to = {1, 0, -8}});

	engine::fixed_step::RunFrame(scene.World(), 1.0F / 60.0F);

	const auto& calls = scene.platform->calls;
	const auto line = std::ranges::find(calls, std::string_view{"line"}, &DrawCall::call);
	const auto first_translucent = std::ranges::find(calls, false, &DrawCall::depth_write);
	ASSERT_NE(line, calls.end());
	ASSERT_NE(first_translucent, calls.end());
	EXPECT_LT(line, first_translucent);
}

} // namespace
