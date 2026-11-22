#include <memory>

#include <gtest/gtest.h>

#include "engine/engine.hpp"

namespace {

// Where the test script reports what it read back from each enum member.
struct EnumProbe {
	int projection{-1};
	int motion{-1};
	int layer{-1};
	int follow_mode{-1};
	int align{-1};
	int direction{-1};
	float linear_damping{0.0F};
	float angular_damping{0.0F};
	bool ccd{false};
	int collision_mask{0};
};

} // namespace

TEST(EnumConstantsScript, ScriptsSetAndReadEnumMembersByConstant) {
	engine::EngineContext ctx(std::make_unique<engine::platform::HeadlessPlatform>());
	flecs::world& world = *ctx.GetWorld();
	ctx.GetModuleRegistry().Import(world, {"physics", "pathfinding"});
	world.component<EnumProbe>()
		.member("projection", &EnumProbe::projection)
		.member("motion", &EnumProbe::motion)
		.member("layer", &EnumProbe::layer)
		.member("follow_mode", &EnumProbe::follow_mode)
		.member("align", &EnumProbe::align)
		.member("direction", &EnumProbe::direction)
		.member("linear_damping", &EnumProbe::linear_damping)
		.member("angular_damping", &EnumProbe::angular_damping)
		.member("ccd", &EnumProbe::ccd)
		.member("collision_mask", &EnumProbe::collision_mask);
	engine::scripting::RegisterComponentForScripts(world, world.component<EnumProbe>());

	const auto probe = world.entity("Probe").set<EnumProbe>({});
	world.entity().child_of(probe).set<engine::scripting::ScriptComponent>({.inline_source = R"(
		void Tick(Entity self, float dt) {
			EnumProbe@ p = self.GetEnumProbe();

			Camera@ cam = self.AddCamera();
			cam.projection = Projection_Orthographic;
			p.projection = cam.projection == Projection_Orthographic ? Projection_Orthographic : -1;

			RigidBody@ rb = self.AddRigidBody();
			rb.motion_type = Motion_Static;
			rb.layer = Layer_Obstacle;
			rb.linear_damping = 0.25f;
			rb.angular_damping = 0.75f;
			rb.enable_ccd = true;
			rb.collision_mask = 6;
			p.motion = rb.motion_type == Motion_Static ? Motion_Static : -1;
			p.layer = rb.layer;
			p.linear_damping = rb.linear_damping;
			p.angular_damping = rb.angular_damping;
			p.ccd = rb.enable_ccd;
			p.collision_mask = rb.collision_mask;

			PathFollow@ pf = self.AddPathFollow();
			pf.mode = PathFollowMode_Direct;
			p.follow_mode = pf.mode == PathFollowMode_Direct ? PathFollowMode_Direct : -1;

			Label@ label = self.AddLabel();
			label.align = TextAlign_Right;
			p.align = label.align == TextAlign_Right ? TextAlign_Right : -1;

			Stack@ stack = self.AddStack();
			stack.direction = StackDirection_Horizontal;
			p.direction = stack.direction == StackDirection_Horizontal ? StackDirection_Horizontal : -1;
		}
	)"});
	for (int i = 0; i < 3; ++i) {
		engine::fixed_step::RunFrame(world, 1.0F / 60.0F);
	}

	const auto& got = world.lookup("Probe").get<EnumProbe>();
	EXPECT_EQ(got.projection, 1);
	EXPECT_EQ(got.motion, 0);
	EXPECT_EQ(got.layer, 3);
	EXPECT_EQ(got.follow_mode, 2);
	EXPECT_EQ(got.align, 2);
	EXPECT_EQ(got.direction, 1);
	EXPECT_FLOAT_EQ(got.linear_damping, 0.25F);
	EXPECT_FLOAT_EQ(got.angular_damping, 0.75F);
	EXPECT_TRUE(got.ccd);
	EXPECT_EQ(got.collision_mask, 6);
}

TEST(EnumConstantsScript, ReflectedEnumMembersSerializeRealValues) {
	engine::EngineContext ctx(std::make_unique<engine::platform::HeadlessPlatform>());
	flecs::world& world = *ctx.GetWorld();
	ctx.GetModuleRegistry().Import(world, {"physics", "pathfinding"});

	const auto e = world.entity()
					   .set<engine::physics::RigidBody>({.motion_type = engine::physics::MotionType::Static})
					   .set<engine::pathfinding::PathFollow>({.mode = engine::pathfinding::PathFollowMode::Direct})
					   .set<engine::ui::Label>({.align = engine::ui::TextAlign::Right})
					   .set<engine::ui::Stack>({.direction = engine::ui::StackDirection::Horizontal});
	const std::string json = e.to_json().c_str();
	EXPECT_NE(json.find("\"motion_type\":\"Static\""), std::string::npos) << json;
	EXPECT_NE(json.find("\"mode\":\"Direct\""), std::string::npos) << json;
	EXPECT_NE(json.find("\"align\":\"Right\""), std::string::npos) << json;
	EXPECT_NE(json.find("\"direction\":\"Horizontal\""), std::string::npos) << json;
}
