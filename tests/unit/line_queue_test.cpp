#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

using engine::core::Rgba;
using engine::render::Line3D;
using engine::render::LineQueue;

namespace {

constexpr Rgba kLinkColor{.r = 40, .g = 220, .b = 255, .a = 255};

// Records every 3D line the render module draws.
class LineRecordingPlatform final : public engine::platform::HeadlessPlatform {
public:
	void DrawLine3D(const glm::vec3 from, const glm::vec3 to, const Rgba color) override {
		drawn->push_back({.from = from, .to = to, .color = color});
	}

	std::shared_ptr<std::vector<Line3D>> drawn = std::make_shared<std::vector<Line3D>>();
};

struct LineWorld {
	std::shared_ptr<std::vector<Line3D>> drawn;
	std::unique_ptr<engine::EngineContext> ctx;

	LineWorld() {
		auto platform = std::make_unique<LineRecordingPlatform>();
		drawn = platform->drawn;
		ctx = std::make_unique<engine::EngineContext>(std::move(platform));
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	// A camera at (0, 5, 5) looking down the -Z axis, so the render module opens a 3D pass.
	void AddCamera() const {
		World()
			.entity("Camera")
			.set<engine::spatial::Transform>({.position = {0.0F, 5.0F, 5.0F}})
			.set<engine::render::Camera>({});
	}

	void Push(const Line3D& line) const { World().get_mut<LineQueue>().lines.push_back(line); }

	void RunFrame() const { engine::fixed_step::RunFrame(World(), 1.0F / 60.0F); }
};

bool SameLine(const Line3D& a, const Line3D& b) {
	return a.from == b.from && a.to == b.to && a.color.r == b.color.r && a.color.g == b.color.g
		   && a.color.b == b.color.b && a.color.a == b.color.a;
}

TEST(LineQueue, AQueuedLineIsDrawnInTheFrameItWasPushed) {
	const LineWorld w;
	w.AddCamera();
	const Line3D link{.from = {-2.0F, 1.0F, 0.0F}, .to = {2.0F, 1.0F, 0.0F}, .color = kLinkColor};

	w.Push(link);
	w.RunFrame();

	ASSERT_EQ(w.drawn->size(), 1U);
	EXPECT_TRUE(SameLine(w.drawn->front(), link));
}

TEST(LineQueue, ALineIsDrawnOnlyOncePerPush) {
	const LineWorld w;
	w.AddCamera();
	w.Push({.from = {0.0F, 0.0F, 0.0F}, .to = {1.0F, 0.0F, 0.0F}, .color = kLinkColor});

	w.RunFrame();
	w.RunFrame();

	EXPECT_EQ(w.drawn->size(), 1U);
	EXPECT_TRUE(w.World().get<LineQueue>().lines.empty());
}

TEST(LineQueue, WithNoCameraLinesAreDroppedUndrawn) {
	const LineWorld w;
	w.Push({.from = {0.0F, 0.0F, 0.0F}, .to = {1.0F, 0.0F, 0.0F}, .color = kLinkColor});

	w.RunFrame();

	EXPECT_TRUE(w.drawn->empty());
	EXPECT_TRUE(w.World().get<LineQueue>().lines.empty());
}

TEST(LineQueue, AScriptsDrawLine3DIsDrawnThatFrame) {
	const LineWorld w;
	w.AddCamera();
	// A script runs on a child of its host entity.
	const auto linker = w.World().entity("Linker");
	w.World().entity().child_of(linker).set<engine::scripting::ScriptComponent>({.inline_source = R"(
		void Tick(Entity self, float dt) {
			Rgba cyan;
			cyan.r = 40;
			cyan.g = 220;
			cyan.b = 255;
			cyan.a = 255;
			DrawLine3D(vec3(-2, 1, 0), vec3(2, 1, 0), cyan);
		}
	)"});

	w.RunFrame();

	ASSERT_EQ(w.drawn->size(), 1U);
	EXPECT_TRUE(SameLine(w.drawn->front(), {.from = {-2.0F, 1.0F, 0.0F}, .to = {2.0F, 1.0F, 0.0F}, .color = kLinkColor}));
}

} // namespace
