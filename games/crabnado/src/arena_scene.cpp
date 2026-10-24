#include "arena_scene.hpp"

#include <algorithm>
#include <format>
#include <string>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr core::Rgba kSandColor{.r = 232, .g = 210, .b = 155, .a = 255};
constexpr core::Rgba kReadoutColor{.r = 245, .g = 245, .b = 235, .a = 255};
// North-up angled top-down view over the whole arena, as in the prototype.
constexpr glm::vec3 kCameraPosition{0.0F, 75.0F, 62.0F};

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
	// clang-format on
}

void ArenaScene::Load(flecs::world& world) {
	sceneRoot_ = world.entity("Arena");
	horde_ = world.entity().child_of(sceneRoot_);
	BuildStage(world);
	BuildUI(world);
}

void ArenaScene::BuildStage(const flecs::world& world) {
	world.set<render::AmbientLight>({.color = {.r = 120, .g = 130, .b = 150, .a = 255}, .intensity = 0.45F});

	const flecs::entity target =
		world.entity("CameraTarget").child_of(sceneRoot_).set<spatial::Transform>({.position = {0.0F, 0.0F, 0.0F}});
	std::ignore = world.entity("Camera")
					  .child_of(sceneRoot_)
					  .set<spatial::Transform>({.position = kCameraPosition})
					  .set<render::Camera>({.fov = 60.0F, .far_plane = 250.0F})
					  .add<render::LookAt>(target);

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

	readout_ = ui::CreateLabel(world, {.x = 16.0F, .y = 12.0F, .w = w - 260.0F, .h = 24.0F}, "", 20.0F)
				   .set<ui::Label>({.font_size = 20.0F, .color = kReadoutColor, .align = ui::TextAlign::Left})
				   .set_name("Readout")
				   .child_of(uiRoot_);

	const std::string help = std::format(
		"[=] spawn {} crabs   [Backspace] clear", constants::kHordeDebugBatch
	);
	std::ignore = ui::CreateLabel(world, {.x = 16.0F, .y = 40.0F, .w = w - 260.0F, .h = 20.0F}, help, 16.0F)
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
}

void ArenaScene::Tick(const flecs::world& world, float) {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	const int enemyCount = world.count<Enemy>();
	world.set<HordeReadout>({.enemy_count = enemyCount, .fps = platform->Fps()});

	const std::string readout = std::format("FPS {}   enemies {}", platform->Fps(), enemyCount);
	if (readout_.has<ui::Label>()) {
		readout_.get_mut<ui::Label>().text = readout;
	}

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
		SpawnBatch(world, enemyCount);
	}
	if (clear || input.WasKeyPressed(input::KeyCode::Backspace)) {
		ClearHorde(world);
	}
}

void ArenaScene::SpawnBatch(const flecs::world& world, const int enemyCount) const {
	const int count = std::min(constants::kHordeDebugBatch, constants::kHordeDebugMax - enemyCount);
	if (count > 0) {
		SpawnCrabs(world, horde_, count);
	}
}

void ArenaScene::ClearHorde(const flecs::world& world) {
	horde_.destruct();
	horde_ = world.entity().child_of(sceneRoot_);
}

void ArenaScene::Unload(flecs::world&) {
	for (flecs::entity* root : {&sceneRoot_, &uiRoot_}) {
		if (*root) {
			root->destruct();
			*root = {};
		}
	}
	horde_ = {};
	readout_ = {};
}

} // namespace crabnado
