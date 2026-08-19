#include "cube_scene.hpp"

#include <string>

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "engine/engine.hpp"
#include "game.hpp"

using namespace engine;

namespace game {

void CubeScene::InitializePipeline(const flecs::world& world) {
	// clang-format off
	pipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::MenuScene>()
					// Re-inject the built-in phase sorting logic:
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();

	pausedPipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::MenuScene>()
					.without<ecs::Pausable>()
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();
	// clang-format on
}

void CubeScene::Load(flecs::world& world) {
	// Restore the debug-menu default each activation — Unload disables it, so without this it would
	// stay off after the first Game→Title→Game round-trip.
	world.get_mut<GameData>().showDebugMenu = true;

	// A single scene-root entity owns every demo entity (via child_of), so Unload can tear the
	// whole tree down with one destruct — no hand-maintained cleanup list to keep in sync.
	sceneRoot_ = world.entity("CubeSceneRoot");
	SetupPhysicsDemo(world);
	BuildUI(world);
}

void CubeScene::SetupPhysicsDemo(const flecs::world& world) const {
	// Create side-on camera
	const auto camera_entity = world.entity("Camera").child_of(sceneRoot_);
	camera_entity.set<ecs::Transform>({.position = {0.0F, 5.0F, 5.0F}});
	camera_entity.set<ecs::WorldTransform>(ecs::MakeWorldTransform({0.0F, 5.0F, 5.0F}, {}, {1.0F, 1.0F, 1.0F}));
	camera_entity.set<render::Camera>(
		{.target = {0.0F, 1.0F, 0.0F},
		 .up = {0.0F, 1.0F, 0.0F},
		 .fov = 60.0F,
		 .aspect_ratio = 16.0F / 9.0F,
		 .near_plane = 0.1F,
		 .far_plane = 100.0F}
	);

	// Ambient fill so shadowed faces stay readable, plus a sun-like directional light that drives
	// Phong shading and casts planar shadows onto the floor's top surface (y = 0.25).
	world.set<render::AmbientLight>({.color = {.r = 90, .g = 105, .b = 130, .a = 255}, .intensity = 0.35F});
	const auto sun = world.entity("SunLight").child_of(sceneRoot_);
	sun.set<render::DirectionalLight>(
		{.direction = {-0.55F, -1.0F, -0.4F},
		 .color = {.r = 255, .g = 245, .b = 220, .a = 255},
		 .intensity = 1.0F,
		 .specular_strength = 0.35F,
		 .shininess = 24.0F,
		 .casts_shadows = true,
		 .shadow_ground_y = 0.25F,
		 .shadow_color = {.r = 8, .g = 10, .b = 14, .a = 115}}
	);

	// Script demo: a child entity whose AngelScript hue-cycles the sun's DirectionalLight colour
	// each tick, showing a script mutating a render component live.
	const auto sun_script = world.entity("SunCycleScript").child_of(sun);
	sun_script.set<scripting::ScriptComponent>({.source_path = assets::ResolveAsset(world, "scripts/sun_cycle.as")});

	// Create floor (static rigid body)
	const auto floor = world.entity("Floor").child_of(sceneRoot_);
	floor.set<ecs::Transform>(
		{.position = {0.0F, 0.0F, 0.0F}, .rotation = {0.0F, 0.0F, 0.0F}, .scale = {10.0F, 0.5F, 10.0F}}
	);
	floor.set<ecs::WorldTransform>(ecs::MakeWorldTransform({0.0F, 0.0F, 0.0F}, {}, {10.0F, 0.5F, 10.0F}));
	floor.set<physics::RigidBody>(
		{.motion_type = physics::MotionType::Static,
		 .mass = 0.0F,
		 .friction = 0.5F,
		 .restitution = 0.0F,
		 .use_gravity = false}
	);
	floor.set<physics::CollisionShape>(
		{.type = physics::ShapeType::Box, .box_half_extents = {5.0F, 0.25F, 5.0F}, .offset = {0.0F, 0.0F, 0.0F}}
	);

	// Render the floor as a unit cube scaled by its WorldTransform (10 x 0.5 x 10). A checker
	// texture makes the surface (and the ball's motion across it) far easier to read. The floor
	// receives shadows but shouldn't cast one, so cast_shadow is disabled.
	floor.set<render::CubePrimitive>({.size = {1.0F, 1.0F, 1.0F}});
	floor.set<render::Material>({.color = {.r = 235, .g = 240, .b = 235, .a = 255}, .cast_shadow = false});
	floor.set<render::AlbedoMap>({.path = assets::ResolveAsset(world, "textures/checker.png")});

	// Create falling cube (dynamic rigid body)
	const auto cube = world.entity("FallingCube").child_of(sceneRoot_);
	cube.set<ecs::Transform>(
		{.position = {0.0F, 3.0F, 0.0F}, .rotation = {0.0F, 0.0F, 0.0F}, .scale = {1.0F, 1.0F, 1.0F}}
	);
	cube.set<ecs::WorldTransform>(ecs::MakeWorldTransform({0.0F, 3.0F, 0.0F}, {}, {1.0F, 1.0F, 1.0F}));
	cube.set<physics::RigidBody>(
		{.motion_type = physics::MotionType::Dynamic,
		 .mass = 1.0F,
		 .linear_damping = 0.05F,
		 .angular_damping = 0.05F,
		 .friction = 0.5F,
		 .restitution = 0.3F,
		 .enable_ccd = false,
		 .use_gravity = true,
		 .gravity_scale = 1.0F}
	);
	cube.set<physics::CollisionShape>(
		{.type = physics::ShapeType::Box, .box_half_extents = {0.5F, 0.5F, 0.5F}, .offset = {0.0F, 0.0F, 0.0F}}
	);
	cube.set<physics::PhysicsVelocity>({});

	// Render the falling cube; physics drives its WorldTransform each frame.
	cube.set<render::CubePrimitive>({.size = {1.0F, 1.0F, 1.0F}});
	cube.set<render::Material>({.color = platform::colors::Boost});

	// Add the checker texture so the T-key script demo can toggle it on/off.
	cube.set<render::AlbedoMap>({.path = assets::ResolveAsset(world, "textures/checker.png")});

	// Attach a jump sound via SoundEffect directly. The ResolveSoundEffect observer fires on
	// OnSet and loads the handle automatically so no separate AudioSource is needed.
	cube.set<audio::SoundEffect>({.path = assets::ResolveAsset(world, "audio/sfx/jump.wav")});

	// First script: jump on space/click, toggle texture with T.
	const auto cube_script = world.entity("CubeJumpScript").child_of(cube);
	cube_script.set<scripting::ScriptComponent>({.source_path = assets::ResolveAsset(world, "scripts/cube_jump.as")});

	// Second script on the same parent entity — demonstrates the multiple-scripts-per-entity
	// pattern. Uses a timer, vec3 constructor, and Vec3_Right; see cube_spin.as for details.
	const auto spin_script = world.entity("CubeSpinScript").child_of(cube);
	spin_script.set<scripting::ScriptComponent>({.source_path = assets::ResolveAsset(world, "scripts/cube_spin.as")});

	// === Particles demo: emitter on the cube (spawns children using the render pipeline) ===
	// P key toggles emitting via particle_toggle.as. The emitter drives the position from the
	// cube's WorldTransform (updated each frame by physics).
	cube.set<particles::ParticleEmitter>(
		{.rate = 30.0F,
		 .particleLifetime = 0.6F,
		 .speed = 2.5F,
		 .startSize = 0.08F,
		 .spread = 0.6F,
		 .direction = {0.0F, 1.0F, 0.0F},
		 .colorStart = {.r = 255, .g = 200, .b = 80, .a = 255},
		 .colorEnd = {.r = 255, .g = 80, .b = 30, .a = 0},
		 .emitting = false} // off by default; P key enables it
	);
	const auto particle_script = world.entity("ParticleToggleScript").child_of(cube);
	particle_script.set<scripting::ScriptComponent>(
		{.source_path = assets::ResolveAsset(world, "scripts/particle_toggle.as")}
	);

	// === Timer + Tween demo: a sphere that pulses colour on a repeating timer ===
	// Placed to the left of the cube. The TimerTweenDemoScript adds Timer + Tween components to
	// this entity via AddTimer()/AddTween() in OnInit; the engine's advance systems drive them.
	const auto timerSphere = world.entity("TimerDemoSphere").child_of(sceneRoot_);
	timerSphere.set<ecs::Transform>({.position = {-2.5F, 0.5F, 0.0F}});
	timerSphere.set<ecs::WorldTransform>(ecs::MakeWorldTransform({-2.5F, 0.5F, 0.0F}, {}, {1.0F, 1.0F, 1.0F}));
	timerSphere.set<render::SpherePrimitive>({.radius = 0.5F});
	timerSphere.set<render::Material>({.color = platform::colors::Boost});
	const auto timer_script = world.entity("TimerTweenDemoScript").child_of(timerSphere);
	timer_script.set<scripting::ScriptComponent>(
		{.source_path = assets::ResolveAsset(world, "scripts/timer_tween_demo.as")}
	);

	// Save-system demo: drives JSON save/load from a script via global verbs (see save_demo.as and
	// src/game/meta_save_example.cpp). Standalone entity — it only uses input + global functions.
	const auto save_demo_script = world.entity("SaveDemoScript").child_of(sceneRoot_);
	save_demo_script.set<scripting::ScriptComponent>(
		{.source_path = assets::ResolveAsset(world, "scripts/save_demo.as")}
	);
}

void CubeScene::BuildUI(const flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());

	uiRoot_ = world.entity("GameUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});

	// HUD container — toggled visible/hidden by Tick based on pause state.
	hud_ = world.entity("GameHUD")
			   .set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}})
			   .set<ui::UIElement>({.z_index = 0, .visible = true})
			   .child_of(uiRoot_);

	std::ignore = ui::CreateLabel(world, {.x = 0.0F, .y = h / 2.0F - 100.0F, .w = w, .h = 48.0F}, "Game Scene", 48.0F)
					  .set<ui::Label>({.text = "Game Scene", .font_size = 48.0F, .color = platform::colors::Title})
					  .child_of(hud_);

	std::ignore = ui::CreateLabel(world, {.x = 0.0F, .y = h / 2.0F, .w = w, .h = 20.0F}, "Press ESC to pause", 16.0F)
					  .set<ui::Label>(
						  {.text = "ESC: pause  |  Space/Click: jump  |  P: toggle particles  |  T: texture",
						   .font_size = 16.0F,
						   .color = platform::colors::Subtle}
					  )
					  .child_of(hud_);

	// Pause menu, composed as a prompt (Modal + Stack + buttons). Starts closed; Tick opens it
	// while the scene is paused.
	pauseModal_ =
		ui::CreatePrompt(
			world,
			{.x = w / 2.0F - 200.0F, .y = h / 2.0F - 130.0F, .w = 400.0F, .h = 240.0F},
			"Paused",
			"",
			{
				{.label = "Resume", .on_click = [](const flecs::entity e) { e.world().remove<scene::Paused>(); }},
				{.label = "Quit to Title",
				 .on_click =
					 [onQuit = onQuitPressed](flecs::entity) {
						 if (onQuit) {
							 onQuit();
						 }
					 }},
			}
		)
			.set<ui::Modal>({.open = false})
			.child_of(uiRoot_);
}

void CubeScene::Tick(const flecs::world& world, float /*deltaTime*/) {
	const bool paused = world.has<scene::Paused>();
	if (hud_ && hud_.has<ui::UIElement>()) {
		hud_.get_mut<ui::UIElement>().visible = !paused;
	}
	if (pauseModal_ && pauseModal_.has<ui::Modal>()) {
		pauseModal_.get_mut<ui::Modal>().open = paused;
	}
}

void CubeScene::HandleInput(const flecs::world& world, const input::InputState& input) {
	// Toggle pause on Escape — input detection only; the pause menu is retained UI driven by Tick.
	if (input.keys[input::KeyCode::Escape].pressed) {
		if (world.has<scene::Paused>()) {
			world.remove<scene::Paused>();
		}
		else {
			world.add<scene::Paused>();
		}
	}
}

void CubeScene::Unload(flecs::world& world) {
	// One destruct tears down the whole demo tree (all entities are children of sceneRoot_).
	if (sceneRoot_) {
		sceneRoot_.destruct();
		sceneRoot_ = flecs::entity{};
	}
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = flecs::entity{};
	}
	// hud_/pauseModal_ were children of uiRoot_ and are already destroyed; clear the stale handles.
	hud_ = flecs::entity{};
	pauseModal_ = flecs::entity{};

	// Clear pause state and reset game data when unloading the scene.
	world.remove<scene::Paused>();
	if (world.has<render::AmbientLight>()) {
		world.remove<render::AmbientLight>();
	}
	world.get_mut<GameData>().showDebugMenu = false;
}

} // namespace game
