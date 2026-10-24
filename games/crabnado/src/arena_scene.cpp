#include "arena_scene.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "claw_snap.hpp"
#include "crabnado.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "run.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr core::Rgba kSandColor{.r = 232, .g = 210, .b = 155, .a = 255};
constexpr core::Rgba kReadoutColor{.r = 245, .g = 245, .b = 235, .a = 255};
constexpr core::Rgba kHudColor{.r = 40, .g = 30, .b = 25, .a = 255};
constexpr core::Rgba kSweptAwayColor{.r = 200, .g = 50, .b = 35, .a = 255};
// Where the crabnado starts, away from the player at the centre.
constexpr glm::vec2 kCrabnadoStart{30.0F, -25.0F};

} // namespace

void ArenaScene::InitializePipeline(const flecs::world& world) {
	// clang-format off
	pipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::MenuScene>()
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

void ArenaScene::Load(flecs::world& world) {
	StartRun(world);
	sceneRoot_ = world.entity("Arena");
	horde_ = world.entity().child_of(sceneRoot_);
	BuildStage(world);
	BuildUI(world);
}

void ArenaScene::BuildStage(const flecs::world& world) {
	world.set<render::AmbientLight>({.color = {.r = 120, .g = 130, .b = 150, .a = 255}, .intensity = 0.45F});

	const flecs::entity player = SpawnPlayer(world, sceneRoot_);
	AddClawSnap(world, player);
	crabnado_ = SpawnCrabnado(world, sceneRoot_, horde_, kCrabnadoStart);

	const glm::vec3 cameraOffset{0.0F, constants::kCameraHeight, constants::kCameraBack};
	std::ignore = world.entity("Camera")
					  .child_of(sceneRoot_)
					  .set<spatial::Transform>({.position = cameraOffset})
					  .set<render::Camera>({.fov = 60.0F, .far_plane = 250.0F})
					  .set<FollowCamera>({.offset = cameraOffset})
					  .add<render::LookAt>(player);

	std::ignore = world.entity("Sun").child_of(sceneRoot_).set<render::DirectionalLight>({});

	std::ignore = world.entity("Sand")
					  .child_of(sceneRoot_)
					  .set<spatial::Transform>({})
					  .set<render::QuadPrimitive>({.size = glm::vec2{2.0F * (constants::kArenaRadius + 30.0F)}})
					  .set<render::Material>({.color = kSandColor, .cast_shadow = false});
}

void ArenaScene::BuildUI(const flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());

	uiRoot_ = world.entity("ArenaUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = w, .h = h}});

	hud_ = ui::CreateLabel(world, {.x = 16.0F, .y = 12.0F, .w = w - 260.0F, .h = 28.0F}, "", 26.0F)
			   .set<ui::Label>({.font_size = 26.0F, .color = kHudColor, .align = ui::TextAlign::Left})
			   .set_name("Hud")
			   .child_of(uiRoot_);

	readout_ = ui::CreateLabel(world, {.x = 16.0F, .y = 46.0F, .w = w - 260.0F, .h = 20.0F}, "", 16.0F)
				   .set<ui::Label>({.font_size = 16.0F, .color = kReadoutColor, .align = ui::TextAlign::Left})
				   .set_name("Readout")
				   .child_of(uiRoot_);

	const std::string help = std::format(
		"[=] throw {} crabs   [Backspace] clear", constants::kHordeDebugBatch
	);
	std::ignore = ui::CreateLabel(world, {.x = 16.0F, .y = 68.0F, .w = w - 260.0F, .h = 20.0F}, help, 16.0F)
					  .set<ui::Label>({.text = help, .font_size = 16.0F, .color = kReadoutColor, .align = ui::TextAlign::Left})
					  .child_of(uiRoot_);

	std::ignore = ui::CreateButton(
					  world,
					  {.x = w - 216.0F, .y = 16.0F, .w = 200.0F, .h = 56.0F},
					  "Title",
					  [onTitle = onTitlePressed](flecs::entity) {
						  if (onTitle) {
							  onTitle();
						  }
					  }
	)
					  .set_name("TitleButton")
					  .child_of(uiRoot_);

	// Placeholder run end until the results screen (ticket 13).
	const std::string sweptAway = "Swept away";
	sweptAway_ = ui::CreateLabel(world, {.x = 0.0F, .y = h / 2.0F - 40.0F, .w = w, .h = 80.0F}, sweptAway, 64.0F)
					 .set<ui::Label>({.text = sweptAway, .font_size = 64.0F, .color = kSweptAwayColor})
					 .set<ui::UIElement>({.visible = false})
					 .set_name("SweptAway")
					 .child_of(uiRoot_);
}

void ArenaScene::Tick(const flecs::world& world, float) {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	const int enemyCount = world.count<Enemy>();
	world.set<HordeReadout>({.enemy_count = enemyCount, .fps = platform->Fps()});

	const std::string readout = std::format("FPS {}   enemies {}", platform->Fps(), enemyCount);
	UpdateHud(world, readout);

	bool spawn = false;
	bool clear = false;
	auto& debugUi = platform->GetDebugUi();
	if (debugUi.BeginPanel("Horde")) {
		debugUi.Text(readout);
		spawn = debugUi.Button(std::format("Spawn {}", constants::kHordeDebugBatch));
		debugUi.SameLine();
		clear = debugUi.Button("Clear");
	}
	debugUi.EndPanel();

	const auto& input = world.get<input::InputState>();
	if (spawn || input.WasKeyPressed(input::KeyCode::Equals)) {
		SpawnBatch(enemyCount);
	}
	if (clear || input.WasKeyPressed(input::KeyCode::Backspace)) {
		ClearHorde(world);
	}
}

void ArenaScene::UpdateHud(const flecs::world& world, const std::string& readout) const {
	const auto& run = world.get<RunState>();
	const flecs::entity player = FindPlayer(world);
	if (!player || !hud_.has<ui::Label>() || !readout_.has<ui::Label>() || !sweptAway_.has<ui::UIElement>()) {
		return;
	}
	const auto& stats = player.get<PlayerStats>();
	const int seconds = static_cast<int>(run.clock);
	hud_.get_mut<ui::Label>().text = std::format(
		"HP {:.0f}/{:.0f}   {}:{:02}   kills {}", std::ceil(stats.hp), stats.max_hp, seconds / 60, seconds % 60, run.kills
	);
	readout_.get_mut<ui::Label>().text = readout;
	sweptAway_.get_mut<ui::UIElement>().visible = run.phase == RunPhase::Lost;
}

void ArenaScene::SpawnBatch(const int enemyCount) const {
	const int count = std::min(constants::kHordeDebugBatch, constants::kHordeDebugMax - enemyCount);
	if (count > 0 && crabnado_) {
		ThrowBurst(crabnado_, count);
	}
}

void ArenaScene::ClearHorde(const flecs::world& world) {
	horde_.destruct();
	horde_ = world.entity().child_of(sceneRoot_);
	crabnado_.add<ThrowsInto>(horde_);
}

void ArenaScene::Unload(flecs::world& world) {
	// A lost run paused the scene; the next scene starts unpaused.
	world.remove<scene::Paused>();
	for (flecs::entity* root : {&sceneRoot_, &uiRoot_}) {
		if (*root) {
			root->destruct();
			*root = {};
		}
	}
	horde_ = {};
	crabnado_ = {};
	hud_ = {};
	readout_ = {};
	sweptAway_ = {};
}

} // namespace crabnado
