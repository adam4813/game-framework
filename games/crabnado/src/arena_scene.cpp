#include "arena_scene.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "arena.hpp"
#include "claw_snap.hpp"
#include "crabnado.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "run.hpp"
#include "upgrades.hpp"
#include "waves.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr core::Rgba kSandColor{.r = 232, .g = 210, .b = 155, .a = 255};
constexpr core::Rgba kReadoutColor{.r = 245, .g = 245, .b = 235, .a = 255};
constexpr core::Rgba kHudColor{.r = 40, .g = 30, .b = 25, .a = 255};
constexpr core::Rgba kSweptAwayColor{.r = 200, .g = 50, .b = 35, .a = 255};
constexpr core::Rgba kShellBarColor{.r = 255, .g = 244, .b = 220, .a = 255};
constexpr core::Rgba kTelegraphColor{.r = 255, .g = 209, .b = 102, .a = 255};
constexpr core::Rgba kStaminaBarColor{.r = 120, .g = 200, .b = 230, .a = 255};
constexpr core::Rgba kExhaustedBarColor{.r = 140, .g = 140, .b = 140, .a = 255};
constexpr float kStaminaBarWidth = 200.0F;
constexpr float kStaminaBarTop = 94.0F;
constexpr float kMoltBarWidth = 400.0F;
constexpr float kMoltModalWidth = 780.0F;
constexpr float kMoltModalHeight = 300.0F;
constexpr float kChoiceWidth = 236.0F;
constexpr float kChoiceGap = 16.0F;
constexpr float kCompassRadius = 56.0F;
constexpr float kCompassMargin = 30.0F;
constexpr float kCompassTop = 90.0F;
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

void ArenaScene::RegisterLoaders(const flecs::world& world) {
	RegisterArena(world);
}

void ArenaScene::Load(flecs::world& world) {
	StartRun(world);
	StartWind(world);
	StartWaves(world);
	sceneRoot_ = world.entity("Arena");
	horde_ = world.entity().child_of(sceneRoot_);
	BuildStage(world);
	BuildUI(world);
}

void ArenaScene::BuildStage(const flecs::world& world) {
	world.set<render::AmbientLight>({.color = {.r = 120, .g = 130, .b = 150, .a = 255}, .intensity = 0.45F});

	BuildArena(world, sceneRoot_);
	const flecs::entity player = SpawnPlayer(world, sceneRoot_);
	AddClawSnap(world, player);
	crabnado_ = SpawnCrabnado(world, sceneRoot_, horde_, kCrabnadoStart);
	SpawnWaterTiles(world, sceneRoot_);
	SpawnWindStreaks(world, sceneRoot_);

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
					  .set<spatial::Transform>({.position = {0.0F, constants::kSandY, 0.0F}})
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

	moltBar_ = ui::CreateProgressBar(
				   world,
				   {.x = (w - kMoltBarWidth) / 2.0F, .y = 14.0F, .w = kMoltBarWidth, .h = 22.0F},
				   {.fill_color = kShellBarColor}
	)
				   .set_name("MoltBar")
				   .child_of(uiRoot_);
	moltLabel_ = ui::CreateLabel(world, {.x = (w - kMoltBarWidth) / 2.0F, .y = 40.0F, .w = kMoltBarWidth, .h = 22.0F}, "", 20.0F)
					 .set<ui::Label>({.font_size = 20.0F, .color = kHudColor})
					 .set_name("MoltLabel")
					 .child_of(uiRoot_);
	telegraph_ = ui::CreateLabel(world, {.x = (w - kMoltBarWidth) / 2.0F, .y = 66.0F, .w = kMoltBarWidth, .h = 28.0F}, "", 26.0F)
					 .set<ui::Label>({.font_size = 26.0F, .color = kTelegraphColor})
					 .set_name("Telegraph")
					 .child_of(uiRoot_);
	staminaBar_ = ui::CreateProgressBar(
					  world,
					  {.x = 16.0F, .y = kStaminaBarTop, .w = kStaminaBarWidth, .h = 16.0F},
					  {.fill_color = kStaminaBarColor}
	)
					  .set_name("StaminaBar")
					  .child_of(uiRoot_);
	stateLabel_ = ui::CreateLabel(world, {.x = 224.0F, .y = kStaminaBarTop - 4.0F, .w = 160.0F, .h = 24.0F}, "", 20.0F)
					  .set<ui::Label>({.font_size = 20.0F, .color = kHudColor, .align = ui::TextAlign::Left})
					  .set_name("StateLabel")
					  .child_of(uiRoot_);
	BuildMoltModal(world, w, h);

	// Under the Title button. The WindCompassDraw system draws it into the UI draw list.
	std::ignore = world.entity("WindCompass")
					  .child_of(uiRoot_)
					  .set<WindCompass>({.center = {w - kCompassMargin - kCompassRadius, kCompassTop + kCompassRadius},
										 .radius = kCompassRadius});

	// Placeholder run end until the results screen (ticket 13).
	const std::string sweptAway = "Swept away";
	sweptAway_ = ui::CreateLabel(world, {.x = 0.0F, .y = h / 2.0F - 40.0F, .w = w, .h = 80.0F}, sweptAway, 64.0F)
					 .set<ui::Label>({.text = sweptAway, .font_size = 64.0F, .color = kSweptAwayColor})
					 .set<ui::UIElement>({.visible = false})
					 .set_name("SweptAway")
					 .child_of(uiRoot_);
}

void ArenaScene::BuildMoltModal(const flecs::world& world, const float width, const float height) {
	const float x = (width - kMoltModalWidth) / 2.0F;
	const float y = (height - kMoltModalHeight) / 2.0F;
	moltModal_ =
		ui::CreateModal(world, {.x = x, .y = y, .w = kMoltModalWidth, .h = kMoltModalHeight}, {.open = false})
			.set_name("MoltModal")
			.child_of(uiRoot_);
	const std::string title = "Molt! Pick an upgrade (1/2/3)";
	std::ignore = ui::CreateLabel(world, {.x = x, .y = y + 20.0F, .w = kMoltModalWidth, .h = 36.0F}, title, 32.0F)
					  .child_of(moltModal_);

	const float rowWidth = kMoltChoices * kChoiceWidth + (kMoltChoices - 1) * kChoiceGap;
	for (int i = 0; i < kMoltChoices; ++i) {
		const float cx = x + (kMoltModalWidth - rowWidth) / 2.0F + static_cast<float>(i) * (kChoiceWidth + kChoiceGap);
		choiceButtons_[i] = ui::CreateButton(
								world,
								{.x = cx, .y = y + 84.0F, .w = kChoiceWidth, .h = 64.0F},
								"",
								[i](const flecs::entity button) { ChooseUpgrade(button.world(), i); }
		)
								.set<ui::UIElement>({})
								.set_name(std::format("Choice{}", i + 1).c_str())
								.child_of(moltModal_);
		choiceDescriptions_[i] =
			ui::CreateLabel(world, {.x = cx, .y = y + 160.0F, .w = kChoiceWidth, .h = 100.0F}, "", 20.0F)
				.set<ui::Label>({.font_size = 20.0F, .wrap = true})
				.set<ui::UIElement>({})
				.child_of(moltModal_);
	}
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
		"HP {:.0f}/{:.0f}   {}:{:02}   kills {}   rock slams {}", std::ceil(stats.hp), stats.max_hp, seconds / 60,
		seconds % 60, run.kills, run.rock_kills
	);
	readout_.get_mut<ui::Label>().text = readout;
	sweptAway_.get_mut<ui::UIElement>().visible = run.phase == RunPhase::Lost;
	UpdateMolt(world);
	UpdateTelegraph(world, player);
	UpdateStamina(player);
}

void ArenaScene::UpdateStamina(const flecs::entity player) const {
	if (!staminaBar_.has<ui::ProgressBar>() || !stateLabel_.has<ui::Label>()) {
		return;
	}
	const auto& stamina = player.get<Stamina>();
	auto& bar = staminaBar_.get_mut<ui::ProgressBar>();
	bar.max = stamina.max;
	bar.value = stamina.value;
	bar.fill_color = stamina.exhausted ? kExhaustedBarColor : kStaminaBarColor;
	const bool surfing = glm::length(player.get<SurfKnock>().velocity) > constants::kSurfingSpeed;
	stateLabel_.get_mut<ui::Label>().text = stamina.dug_in ? "DUG IN" : (surfing ? "SURFING" : "");
}

void ArenaScene::UpdateTelegraph(const flecs::world& world, const flecs::entity player) const {
	if (!telegraph_.has<ui::Label>()) {
		return;
	}
	const glm::vec3 p = player.get<spatial::Transform>().position;
	const auto warning = NextBigCrestIn(world.get<WaveField>(), {p.x, p.z}, constants::kBigCrestWarning);
	telegraph_.get_mut<ui::Label>().text = warning ? std::format("BIG CREST in {:.1f} s", *warning) : std::string{};
}

void ArenaScene::UpdateMolt(const flecs::world& world) const {
	if (!moltBar_.has<ui::ProgressBar>() || !moltLabel_.has<ui::Label>() || !moltModal_.has<ui::Modal>()) {
		return;
	}
	const auto& run = world.get<RunState>();
	auto& bar = moltBar_.get_mut<ui::ProgressBar>();
	bar.max = static_cast<float>(run.shells_needed);
	bar.value = static_cast<float>(run.shells);
	moltLabel_.get_mut<ui::Label>().text =
		std::format("Molt {}   {}/{} shells", run.molt_level, run.shells, run.shells_needed);

	const bool molting = run.phase == RunPhase::Molting;
	moltModal_.get_mut<ui::Modal>().open = molting;
	if (!molting) {
		return;
	}
	const auto& offer = world.get<MoltOffer>();
	for (int i = 0; i < kMoltChoices; ++i) {
		const Upgrade* upgrade = FindUpgrade(world, offer.ids[i]);
		const flecs::entity button = choiceButtons_[i];
		const flecs::entity description = choiceDescriptions_[i];
		if (!button.has<ui::UIElement>() || !description.has<ui::UIElement>()) {
			continue;
		}
		button.get_mut<ui::UIElement>().visible = upgrade != nullptr;
		description.get_mut<ui::UIElement>().visible = upgrade != nullptr;
		if (upgrade != nullptr) {
			button.get_mut<ui::Button>().label = std::format("{}. {}", i + 1, upgrade->name);
			description.get_mut<ui::Label>().text = upgrade->description;
		}
	}
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
	ClearArena(world);
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
	moltBar_ = {};
	moltLabel_ = {};
	telegraph_ = {};
	moltModal_ = {};
	choiceButtons_ = {};
	choiceDescriptions_ = {};
}

} // namespace crabnado
