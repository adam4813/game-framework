#include "arena_scene.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include "bubbles.hpp"
#include "crabnado_constants.hpp"
#include "engine/engine.hpp"
#include "arena.hpp"
#include "claw_snap.hpp"
#include "crabnado.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "run.hpp"
#include "sfx.hpp"
#include "enemy_types.hpp"
#include "upgrades.hpp"
#include "riptide.hpp"
#include "waves.hpp"
#include "wind.hpp"

using namespace engine;

namespace crabnado {

namespace {

constexpr core::Rgba kSandColor{.r = 232, .g = 210, .b = 155, .a = 255};
constexpr core::Rgba kReadoutColor{.r = 245, .g = 245, .b = 235, .a = 255};
constexpr core::Rgba kHudColor{.r = 40, .g = 30, .b = 25, .a = 255};
constexpr core::Rgba kResultsHeadingColor{.r = 200, .g = 50, .b = 35, .a = 255};
constexpr core::Rgba kShellBarColor{.r = 255, .g = 244, .b = 220, .a = 255};
constexpr core::Rgba kTelegraphColor{.r = 255, .g = 209, .b = 102, .a = 255};
constexpr core::Rgba kVictoryHeadingColor{.r = 40, .g = 150, .b = 80, .a = 255};
constexpr core::Rgba kBossColor{.r = 170, .g = 40, .b = 30, .a = 255};
constexpr core::Rgba kBannerColor{.r = 255, .g = 244, .b = 220, .a = 255};
constexpr core::Rgba kRogueColor{.r = 235, .g = 60, .b = 50, .a = 255};
constexpr core::Rgba kStaminaBarColor{.r = 120, .g = 200, .b = 230, .a = 255};
constexpr core::Rgba kExhaustedBarColor{.r = 140, .g = 140, .b = 140, .a = 255};
constexpr float kStaminaBarWidth = 200.0F;
constexpr float kStaminaBarTop = 94.0F;
constexpr float kMoltBarWidth = 400.0F;
constexpr float kMoltModalWidth = 780.0F;
constexpr float kMoltModalHeight = 300.0F;
constexpr float kResultsModalWidth = 520.0F;
constexpr float kResultsModalHeight = 380.0F;
constexpr float kPauseModalWidth = 360.0F;
constexpr float kPauseModalHeight = 240.0F;
constexpr float kModalButtonWidth = 200.0F;
constexpr float kModalButtonHeight = 56.0F;
constexpr float kChoiceWidth = 236.0F;
constexpr float kChoiceGap = 16.0F;
// During the boss the telegraph always shows the next big crest, not only in the last seconds.
constexpr float kBossCrestLookahead = 3.0F * constants::kWavePeriod;
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
	LoadEnemyTypes(world, world.get<platform::PlatformRef>().ptr->AssetDirectory() + std::string{constants::kEnemiesPath});
	StartWind(world);
	StartWaves(world);
	sceneRoot_ = world.entity("Arena");
	crestCued_ = false;
	music_ = world.entity("Music").child_of(sceneRoot_).set<audio::Music>(
		{.path = assets::ResolveAsset(world, "audio/crabnado/music.ogg"), .loop = true, .volume = constants::kMusicVolume}
	);
	horde_ = world.entity().child_of(sceneRoot_);
	BuildStage(world);
	BuildUI(world);
}

void ArenaScene::BuildStage(const flecs::world& world) {
	world.set<render::AmbientLight>({.color = {.r = 120, .g = 130, .b = 150, .a = 255}, .intensity = 0.45F});

	// A scene Load runs in a deferred frame, so the level's entities wouldn't exist for BuildArena to cache.
	const bool deferred = world.is_deferred();
	if (deferred) {
		world.defer_suspend();
	}
	BuildArena(world, sceneRoot_);
	if (deferred) {
		world.defer_resume();
	}
	const flecs::entity player = SpawnPlayer(world, sceneRoot_);
	AddClawSnap(world, player);
	AddBubbles(player);
	AddRiptide(player);
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
	bossLabel_ = ui::CreateLabel(world, {.x = (w - kMoltBarWidth) / 2.0F, .y = 98.0F, .w = kMoltBarWidth, .h = 28.0F}, "", 24.0F)
					 .set<ui::Label>({.font_size = 24.0F, .color = kBossColor})
					 .set_name("BossLabel")
					 .child_of(uiRoot_);
	bannerLabel_ = ui::CreateLabel(world, {.x = 0.0F, .y = h * 0.28F, .w = w, .h = 44.0F}, "", 38.0F)
					   .set<ui::Label>({.font_size = 38.0F, .color = kBannerColor})
					   .set_name("BannerLabel")
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

	BuildResultsModal(world, w, h);
	BuildPauseModal(world, w, h);
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

void ArenaScene::BuildResultsModal(const flecs::world& world, const float width, const float height) {
	const float x = (width - kResultsModalWidth) / 2.0F;
	const float y = (height - kResultsModalHeight) / 2.0F;
	resultsModal_ =
		ui::CreateModal(world, {.x = x, .y = y, .w = kResultsModalWidth, .h = kResultsModalHeight}, {.open = false})
			.set_name("ResultsModal")
			.child_of(uiRoot_);
	resultsHeading_ = ui::CreateLabel(world, {.x = x, .y = y + 24.0F, .w = kResultsModalWidth, .h = 56.0F}, "", 48.0F)
						  .set<ui::Label>({.font_size = 48.0F, .color = kResultsHeadingColor})
						  .child_of(resultsModal_);
	resultsStats_ =
		ui::CreateLabel(world, {.x = x, .y = y + 96.0F, .w = kResultsModalWidth, .h = 120.0F}, "", 26.0F)
			.set<ui::Label>({.font_size = 26.0F})
			.child_of(resultsModal_);

	const float rowWidth = 2.0F * kModalButtonWidth + kChoiceGap;
	const float bx = x + (kResultsModalWidth - rowWidth) / 2.0F;
	const float by = y + kResultsModalHeight - kModalButtonHeight - 28.0F;
	std::ignore = ui::CreateButton(
					  world,
					  {.x = bx, .y = by, .w = kModalButtonWidth, .h = kModalButtonHeight},
					  "Retry",
					  [](const flecs::entity button) {
						  // Outside the click's deferred frame: there the reload's remove and re-add of Active
						  // collapse into nothing and the scene is never unloaded.
						  button.world().get<EngineContextRef>().ptr->GetDeferredTaskQueue().Enqueue(
							  [world = flecs::world(button.world())] { scene::ReloadScene<ArenaSceneTag>(world); }
						  );
					  }
	)
					  .set_name("RetryButton")
					  .child_of(resultsModal_);
	std::ignore = ui::CreateButton(
					  world,
					  {.x = bx + kModalButtonWidth + kChoiceGap, .y = by, .w = kModalButtonWidth, .h = kModalButtonHeight},
					  "Title",
					  [onTitle = onTitlePressed](flecs::entity) {
						  if (onTitle) {
							  onTitle();
						  }
					  }
	)
					  .set_name("ResultsTitleButton")
					  .child_of(resultsModal_);
}

void ArenaScene::BuildPauseModal(const flecs::world& world, const float width, const float height) {
	const float x = (width - kPauseModalWidth) / 2.0F;
	const float y = (height - kPauseModalHeight) / 2.0F;
	pauseModal_ = ui::CreateModal(world, {.x = x, .y = y, .w = kPauseModalWidth, .h = kPauseModalHeight}, {.open = false})
					  .set_name("PauseModal")
					  .child_of(uiRoot_);
	std::ignore =
		ui::CreateLabel(world, {.x = x, .y = y + 20.0F, .w = kPauseModalWidth, .h = 44.0F}, "Paused", 40.0F)
			.child_of(pauseModal_);
	const float bx = x + (kPauseModalWidth - kModalButtonWidth) / 2.0F;
	std::ignore = ui::CreateButton(
					  world,
					  {.x = bx, .y = y + 88.0F, .w = kModalButtonWidth, .h = kModalButtonHeight},
					  "Resume",
					  [](const flecs::entity button) { SetMenuPaused(button.world(), false); }
	)
					  .set_name("ResumeButton")
					  .child_of(pauseModal_);
	std::ignore = ui::CreateButton(
					  world,
					  {.x = bx, .y = y + 160.0F, .w = kModalButtonWidth, .h = kModalButtonHeight},
					  "Quit to title",
					  [onTitle = onTitlePressed](flecs::entity) {
						  if (onTitle) {
							  onTitle();
						  }
					  }
	)
					  .set_name("QuitButton")
					  .child_of(pauseModal_);
}

void ArenaScene::UpdateResults(const flecs::world& world) const {
	if (!resultsModal_.has<ui::Modal>() || !pauseModal_.has<ui::Modal>() || !resultsHeading_.has<ui::Label>() ||
		!resultsStats_.has<ui::Label>()) {
		return;
	}
	const auto& run = world.get<RunState>();
	const bool won = run.phase == RunPhase::Won;
	const bool ended = won || run.phase == RunPhase::Lost;
	resultsModal_.get_mut<ui::Modal>().open = ended;
	pauseModal_.get_mut<ui::Modal>().open = run.menu_paused;
	if (music_.has<audio::Music>()) {
		music_.get_mut<audio::Music>().volume = ended ? constants::kEndedMusicVolume : constants::kMusicVolume;
	}
	if (!ended) {
		return;
	}
	const int seconds = static_cast<int>(run.clock);
	auto& heading = resultsHeading_.get_mut<ui::Label>();
	heading.text = won ? "Victory!" : "Swept away";
	heading.color = won ? kVictoryHeadingColor : kResultsHeadingColor;
	resultsStats_.get_mut<ui::Label>().text = std::format(
		"{}Time {}  {}:{:02}\nKills  {}\nRock slams  {}\nMolt level  {}", won ? "The crabnado breaks apart!\n" : "",
		won ? "to win" : "survived", seconds / 60, seconds % 60, run.kills, run.rock_kills, run.molt_level
	);
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
	if (!player || !hud_.has<ui::Label>() || !readout_.has<ui::Label>()) {
		return;
	}
	const auto& stats = player.get<PlayerStats>();
	const int seconds = static_cast<int>(run.clock);
	hud_.get_mut<ui::Label>().text = std::format(
		"HP {:.0f}/{:.0f}   {}:{:02} {}   kills {}   rock slams {}", std::ceil(stats.hp), stats.max_hp, seconds / 60,
		seconds % 60, WaveStageName(run.clock), run.kills, run.rock_kills
	);
	readout_.get_mut<ui::Label>().text = readout;
	UpdateResults(world);
	UpdateMolt(world);
	UpdateTelegraph(world, player);
	UpdateBoss(world);
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

void ArenaScene::UpdateBoss(const flecs::world& world) const {
	if (!bossLabel_.has<ui::Label>() || !bannerLabel_.has<ui::Label>()) {
		return;
	}
	const auto& run = world.get<RunState>();
	const bool boss = run.phase == RunPhase::Boss;
	bossLabel_.get_mut<ui::Label>().text =
		boss ? std::format("CRABNADO \xC2\xB7 slams left {}", constants::kBossSlamsToWin - run.slams) : "";
	std::string banner;
	if (boss && run.banner_timer > 0.0F) {
		banner = run.slams == 0 ? "The crabnado comes ashore! Slam it into the rocks"
								: std::format("SLAM! {}/{}", run.slams, constants::kBossSlamsToWin);
	}
	bannerLabel_.get_mut<ui::Label>().text = banner;
}

void ArenaScene::UpdateTelegraph(const flecs::world& world, const flecs::entity player) const {
	if (!telegraph_.has<ui::Label>()) {
		return;
	}
	const glm::vec3 p = player.get<spatial::Transform>().position;
	const auto& field = world.get<WaveField>();
	const glm::vec2 position{p.x, p.z};
	auto& label = telegraph_.get_mut<ui::Label>();
	label.color = kTelegraphColor;
	const bool crestComing = NextBigCrestIn(field, position, constants::kBigCrestWarning).has_value();
	if (crestComing && !crestCued_) {
		PlaySfx(world, Sfx::Crest);
	}
	crestCued_ = crestComing;
	if (const auto rogue = NextRogueIn(field, position, constants::kRogueWarning)) {
		label.text = std::format("ROGUE WAVE in {:.1f} s", *rogue);
		label.color = kRogueColor;
	} else if (const auto riptide = BoostedCrestIn(field, position, constants::kBigCrestWarning)) {
		label.text = std::format("RIPTIDE CREST in {:.1f} s", *riptide);
	} else if (const auto crest = NextBigCrestIn(
				   field, position, world.get<RunState>().phase == RunPhase::Boss ? kBossCrestLookahead : constants::kBigCrestWarning
			   )) {
		label.text = std::format("BIG CREST in {:.1f} s", *crest);
	} else {
		label.text = field.notice_timer > 0.0F ? "A cross-swell is building" : "";
	}
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
	music_ = {};
	hud_ = {};
	readout_ = {};
	resultsModal_ = {};
	resultsHeading_ = {};
	resultsStats_ = {};
	pauseModal_ = {};
	moltBar_ = {};
	moltLabel_ = {};
	telegraph_ = {};
	bossLabel_ = {};
	bannerLabel_ = {};
	moltModal_ = {};
	choiceButtons_ = {};
	choiceDescriptions_ = {};
}

} // namespace crabnado
