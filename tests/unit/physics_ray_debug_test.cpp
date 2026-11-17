#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "engine/physics/jolt/jolt_backend.hpp"
#include "engine/physics/jolt/jolt_debug_renderer.hpp"

#ifdef ENGINE_PHYSICS_DEBUG_RENDERER

using engine::physics::JoltBackend;
using engine::physics::JoltDebugRendererAdapter;
using engine::physics::Ray;
using engine::render::Line3D;

namespace {

constexpr float kTolerance = 1e-3F;

// A floor whose top face is at y = 0, and a 1 m box resting on it at x = 4.
constexpr auto kLevel = R"({"entities": [
	{"name": "Floor", "components": {
		"rigid_body": {"motion_type": "static"},
		"collision_shape": {"type": "box", "box_half_extents": [50.0, 0.25, 50.0]},
		"transform": {"position": [0.0, -0.25, 0.0]}
	}},
	{"name": "Box", "components": {
		"rigid_body": {"motion_type": "static"},
		"collision_shape": {"type": "box", "box_half_extents": [0.5, 0.5, 0.5]},
		"transform": {"position": [4.0, 0.5, 0.0]}
	}}
]})";

// Straight down onto the floor at x = 2, from y = 5.
constexpr Ray kDown{.origin = {2.0F, 5.0F, 0.0F}, .direction = {0.0F, -1.0F, 0.0F}, .max_distance = 10.0F};
// Straight up from the floor: hits nothing.
constexpr Ray kUp{.origin = {2.0F, 1.0F, 0.0F}, .direction = {0.0F, 2.0F, 0.0F}, .max_distance = 3.0F};

// A headless physics world whose backend records raycasts into an enabled adapter. A headless world installs no debug
// renderer of its own (it has no window), so the test installs one. The adapter is created after the physics module,
// as the engine does: Jolt's allocator is set up by the backend's Initialize, and the adapter allocates through it.
struct RayDebugWorld {
	std::unique_ptr<engine::EngineContext> ctx;
	std::unique_ptr<JoltDebugRendererAdapter> debug;

	RayDebugWorld() {
		ctx = std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());
		ctx->GetModuleRegistry().Import(World(), {"physics"});
		engine::level::LoadLevelFromSource(World(), kLevel);
		engine::fixed_step::RunFrame(World(), 1.0F / 60.0F);
		debug = std::make_unique<JoltDebugRendererAdapter>();
		World().get_mut<JoltBackend>().SetDebugRenderer(debug.get());
		debug->SetEnabled(true);
	}

	~RayDebugWorld() { World().get_mut<JoltBackend>().SetDebugRenderer(nullptr); }

	RayDebugWorld(const RayDebugWorld&) = delete;
	RayDebugWorld& operator=(const RayDebugWorld&) = delete;

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }

	std::vector<Line3D> Flush() {
		std::vector<Line3D> lines;
		debug->Flush(lines);
		return lines;
	}
};

bool Near(const glm::vec3 a, const glm::vec3 b) { return glm::length(a - b) < kTolerance; }

bool HasLine(const std::vector<Line3D>& lines, const glm::vec3 from, const glm::vec3 to) {
	for (const auto& l : lines) {
		if (Near(l.from, from) && Near(l.to, to)) return true;
	}
	return false;
}

TEST(PhysicsRayDebug, AHitDrawsTheRayToTheHitPointAndTheNormal) {
	RayDebugWorld w;
	ASSERT_TRUE(engine::physics::Raycast(w.World(), kDown).has_value());

	const auto lines = w.Flush();
	EXPECT_TRUE(HasLine(lines, {2, 5, 0}, {2, 0, 0}));    // origin to the hit point
	EXPECT_TRUE(HasLine(lines, {2, 0, 0}, {2, -5, 0}));   // the rest of the ray
	EXPECT_TRUE(HasLine(lines, {2, 0, 0}, {2, 0.5F, 0})); // the floor's normal, up
}

TEST(PhysicsRayDebug, AMissDrawsTheWholeRayOnly) {
	RayDebugWorld w;
	ASSERT_FALSE(engine::physics::Raycast(w.World(), kUp).has_value());

	const auto lines = w.Flush();
	ASSERT_EQ(lines.size(), 1U);
	EXPECT_TRUE(HasLine(lines, {2, 1, 0}, {2, 4, 0})); // max_distance along the normalized direction
}

TEST(PhysicsRayDebug, RaycastAllMarksEveryHit) {
	RayDebugWorld w;
	// Along +x at y = 0.5 from x = 0: enters the box at x = 3.5 and stays above the floor.
	const Ray along{.origin = {0.0F, 0.5F, 0.0F}, .direction = {1.0F, 0.0F, 0.0F}, .max_distance = 10.0F};
	const auto hits = engine::physics::RaycastAll(w.World(), along);
	ASSERT_EQ(hits.size(), 1U);

	const auto lines = w.Flush();
	EXPECT_TRUE(HasLine(lines, {0, 0.5F, 0}, {3.5F, 0.5F, 0}));
	EXPECT_TRUE(HasLine(lines, {3.5F, 0.5F, 0}, {3.0F, 0.5F, 0})); // the box face's normal, back along -x
}

TEST(PhysicsRayDebug, ARayStaysUntilALaterFrameCastsAgain) {
	RayDebugWorld w;
	std::ignore = engine::physics::Raycast(w.World(), kDown);
	const auto first = w.Flush();
	EXPECT_EQ(w.Flush().size(), first.size()); // no new cast: still shown

	std::ignore = engine::physics::Raycast(w.World(), kUp);
	const auto replaced = w.Flush();
	ASSERT_EQ(replaced.size(), 1U);
	EXPECT_TRUE(HasLine(replaced, {2, 1, 0}, {2, 4, 0}));
}

TEST(PhysicsRayDebug, NothingIsRecordedWhileDisabled) {
	RayDebugWorld w;
	std::ignore = engine::physics::Raycast(w.World(), kDown);
	w.debug->SetEnabled(false);
	EXPECT_TRUE(w.Flush().empty()); // turning it off drops what was recorded

	std::ignore = engine::physics::Raycast(w.World(), kDown);
	EXPECT_TRUE(w.Flush().empty());
}

} // namespace

#endif // ENGINE_PHYSICS_DEBUG_RENDERER
