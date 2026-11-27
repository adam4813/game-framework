#include "worldgen_preview_scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>


#include "engine/engine.hpp"
#include "farm_world_gen.hpp"
#include "map_export.hpp"
#include "world_gen.hpp"
#include "world_report.hpp"

using namespace engine;

namespace game::farm {
namespace {

constexpr float kPad = 14.0f;
constexpr float kTopBar = 54.0f;
constexpr float kRightPanelW = 460.0f;
constexpr float kParamsPanelW = 340.0f; // left column of live-tunable generation-parameter sliders
constexpr float kMinZoom = 0.6f;
constexpr float kMaxZoom = 40.0f;

// Compact a slider value for its label: trim trailing zeros so "0.550" reads "0.55" and "8.000"
// reads "8", keeping the parameter panel legible.
std::string FormatFloat(const float value) {
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.3f", static_cast<double>(value));
	std::string s{buf};
	if (s.find('.') != std::string::npos) {
		while (!s.empty() && s.back() == '0') {
			s.pop_back();
		}
		if (!s.empty() && s.back() == '.') {
			s.pop_back();
		}
	}
	return s;
}

core::Rgba BiomeColour(const Biome b) {
	switch (b) {
	case Biome::Ocean: return {.r = 34, .g = 64, .b = 120, .a = 255};
	case Biome::Lake: return {.r = 60, .g = 110, .b = 180, .a = 255};
	case Biome::River: return {.r = 90, .g = 150, .b = 210, .a = 255};
	case Biome::Beach: return {.r = 210, .g = 200, .b = 140, .a = 255};
	case Biome::Plains: return {.r = 108, .g = 160, .b = 80, .a = 255};
	case Biome::Forest: return {.r = 52, .g = 104, .b = 56, .a = 255};
	case Biome::Hills: return {.r = 140, .g = 120, .b = 80, .a = 255};
	case Biome::Mountain: return {.r = 150, .g = 150, .b = 158, .a = 255};
	default: break;
	}
	return {.r = 0, .g = 0, .b = 0, .a = 255};
}

// Terrain shown for a water/river tile before that layer is "revealed" by the timeline — fall back
// to a plausible land band from elevation so early steps read as bare landmass.
Biome LandBand(const Grid& grid, const int x, const int y) {
	const float e = grid.ElevationAt(x, y);
	if (e <= 0.42f) {
		return Biome::Beach;
	}
	if (e <= 0.55f) {
		return Biome::Plains;
	}
	return Biome::Forest;
}

int IndexOfKind(const GenerationTimeline& timeline, const SnapshotKind kind) {
	for (int i = 0; i < static_cast<int>(timeline.steps.size()); ++i) {
		if (timeline.steps[i].kind == kind) {
			return i;
		}
	}
	return 0;
}

} // namespace

void WorldGenPreviewScene::InitializePipeline(const flecs::world& world) {
	pipeline_ = ecs::FramePipeline(world).without<scene::GameScene>().build();
	fixedStepPipeline_ = ecs::FixedStepPipeline(world).without<scene::GameScene>().build();
}

void WorldGenPreviewScene::Load(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto sw = static_cast<float>(platform->Width());
	const auto sh = static_cast<float>(platform->Height());
	const float rightX = sw - kRightPanelW - kPad;
	const float mapX = kPad + kParamsPanelW + kPad;
	mapRect_ = {.x = mapX, .y = kTopBar, .w = rightX - mapX - kPad, .h = sh - kTopBar - kPad};

	// Preview the real play world: start from the canonical gameplay parameters so what the player
	// sees here is the world they will actually play (see games/farm/docs/adr/0001).
	params_ = CanonicalFarmParams();

	BuildUI(world);
	Regenerate(world);
}

void WorldGenPreviewScene::Unload(flecs::world&) {
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = {};
	}
	timelineScroll_ = {};
	timelineList_ = {};
	stepLabel_ = {};
	exportStatus_ = {};
	paramsScroll_ = {};
	paramsList_ = {};
	stepButtons_.clear();
}

void WorldGenPreviewScene::ExportReport(const flecs::world& world) {
	const std::string filename = "world_report_seed" + std::to_string(seed_) + ".md";
	std::ofstream file(filename, std::ios::binary);
	bool ok = false;
	if (file) {
		file << FormatWorldReport(world_);
		ok = static_cast<bool>(file);
	}
	if (ok) {
		engine::ecs::Log(world).info("[WorldGenPreview] Exported world report to {}", filename);
	}
	else {
		engine::ecs::Log(world).warn("[WorldGenPreview] Failed to write world report to {}", filename);
	}
	if (exportStatus_ && exportStatus_.has<ui::Label>()) {
		exportStatus_.get_mut<ui::Label>().text = ok ? ("Saved " + filename) : ("Failed: " + filename);
	}
}

void WorldGenPreviewScene::ExportImage(const flecs::world& world) {
	const std::string filename = "world_map_seed" + std::to_string(seed_) + ".png";
	const bool ok = WriteWorldImagePng(world_, filename, 6);
	if (ok) {
		engine::ecs::Log(world).info("[WorldGenPreview] Exported world map image to {}", filename);
	}
	else {
		engine::ecs::Log(world).warn("[WorldGenPreview] Failed to write world map image to {}", filename);
	}
	if (exportStatus_ && exportStatus_.has<ui::Label>()) {
		exportStatus_.get_mut<ui::Label>().text = ok ? ("Saved " + filename) : ("Failed: " + filename);
	}
}

void WorldGenPreviewScene::Regenerate(flecs::world& world) {
	// Generate the full play world (canonical params + building/tree/creature catalogs) so the biome
	// map previews exactly what FarmWorldScene will build. Publish it as the CurrentWorld singleton:
	// the preview and the in-game farm are two lenses on this one state (see games/farm/docs/adr/0001).
	world_ = GenerateFarmWorld(world, seed_, params_);
	world.set<CurrentWorld>({world_});
	waterStepIndex_ = IndexOfKind(world_.timeline, SnapshotKind::WaterBodies);
	riverStepIndex_ = IndexOfKind(world_.timeline, SnapshotKind::Rivers);
	currentStep_ = static_cast<int>(world_.timeline.steps.size()) - 1; // show the fully generated world
	FitView();
	RebuildTimelineList(world);
	SetStep(currentStep_);
}

void WorldGenPreviewScene::FitView() {
	const float fitX = mapRect_.w / static_cast<float>(std::max(1, world_.grid.width));
	const float fitY = mapRect_.h / static_cast<float>(std::max(1, world_.grid.height));
	zoom_ = std::clamp(std::min(fitX, fitY), kMinZoom, kMaxZoom);
	panCentreCell_ = {static_cast<float>(world_.grid.width) * 0.5f, static_cast<float>(world_.grid.height) * 0.5f};
}

void WorldGenPreviewScene::SetStep(const int step) {
	currentStep_ = std::clamp(step, 0, static_cast<int>(world_.timeline.steps.size()) - 1);
	if (stepLabel_ && stepLabel_.has<ui::Label>()) {
		const std::string label = "Step "
								  + std::to_string(currentStep_ + 1)
								  + "/"
								  + std::to_string(world_.timeline.steps.size())
								  + ": "
								  + world_.timeline.steps[static_cast<size_t>(currentStep_)].label;
		stepLabel_.get_mut<ui::Label>().text = label;
	}
	// Highlight the active step button; mute the rest.
	for (int i = 0; i < static_cast<int>(stepButtons_.size()); ++i) {
		flecs::entity btn = stepButtons_[static_cast<size_t>(i)];
		if (!btn || !btn.has<ui::Button>()) {
			continue;
		}
		ui::Button& b = btn.get_mut<ui::Button>();
		if (i == currentStep_) {
			b.normal = core::colors::Accent;
			b.text_color = core::colors::White;
		}
		else {
			b.normal = core::colors::PanelBg;
			b.text_color = core::colors::Subtle;
		}
	}

	// Scroll so the active item is centred, clamped immediately to the valid range so it never
	// over-scrolls for a frame (which caused a one-frame jump/tear near the ends).
	if (timelineScroll_ && timelineScroll_.has<ui::ScrollRect>() && timelineScroll_.has<ui::UIRect>()) {
		constexpr float rowStride = 30.0f; // 26 button height + 4 spacing
		constexpr float padding = 4.0f;
		const float viewportH = timelineScroll_.get<ui::UIRect>().rect.h;
		const float systemContentH = timelineScroll_.get<ui::ScrollRect>().content_size.y;
		const float contentH =
			systemContentH > 0.0f ? systemContentH : padding + static_cast<float>(stepButtons_.size()) * rowStride;
		const float maxScroll = std::max(0.0f, contentH - viewportH);
		const float itemCentre = padding + static_cast<float>(currentStep_) * rowStride + 13.0f;
		timelineScroll_.get_mut<ui::ScrollRect>().scroll.y = std::clamp(itemCentre - viewportH * 0.5f, 0.0f, maxScroll);
	}
}

glm::vec2 WorldGenPreviewScene::CellToScreen(const glm::vec2 cell) const {
	const float halfW = mapRect_.w * 0.5f;
	const float halfH = mapRect_.h * 0.5f;
	return {
		mapRect_.x + halfW + (cell.x - panCentreCell_.x) * zoom_,
		mapRect_.y + halfH + (cell.y - panCentreCell_.y) * zoom_
	};
}

void WorldGenPreviewScene::BuildUI(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto sw = static_cast<float>(platform->Width());
	const auto sh = static_cast<float>(platform->Height());
	const float rightX = sw - kRightPanelW - kPad;

	uiRoot_ = world.entity("WorldGenPreviewUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0f, .y = 0.0f, .w = sw, .h = sh}});

	// ── Top bar: title, step label, seed field, regenerate, back ──
	std::ignore = ui::CreateLabel(world, {.x = kPad, .y = 10.0f, .w = 260.0f, .h = 30.0f}, "World Gen Preview", 24.0f)
					  .set<ui::Label>(
						  {.text = "World Gen Preview",
						   .font_size = 24.0f,
						   .color = core::colors::Title,
						   .align = ui::TextAlign::Left}
					  )
					  .child_of(uiRoot_);

	stepLabel_ =
		ui::CreateLabel(world, {.x = 290.0f, .y = 16.0f, .w = rightX - 300.0f, .h = 24.0f}, "", 16.0f)
			.set<ui::Label>({.text = "", .font_size = 16.0f, .color = core::colors::Text, .align = ui::TextAlign::Left})
			.child_of(uiRoot_);

	// ── Right column: timeline panel ──
	const float colY = kTopBar;
	const float colH = sh - colY - kPad;
	const auto panel = ui::CreatePanel(
						   world,
						   {.x = rightX, .y = colY, .w = kRightPanelW, .h = colH},
						   {.color = core::colors::PanelBg, .roundness = 0.03f}
	)
						   .child_of(uiRoot_);

	std::ignore = ui::CreateLabel(
					  world,
					  {.x = rightX + kPad, .y = colY + 8.0f, .w = kRightPanelW - kPad * 2, .h = 24.0f},
					  "Generation Timeline",
					  18.0f
	)
					  .set<ui::Label>({.text = "Generation Timeline", .font_size = 18.0f, .color = core::colors::Title})
					  .child_of(panel);

	// Seed field + regenerate.
	std::ignore =
		ui::CreateLabel(world, {.x = rightX + kPad, .y = colY + 38.0f, .w = 48.0f, .h = 28.0f}, "Seed", 15.0f)
			.set<ui::Label>(
				{.text = "Seed", .font_size = 15.0f, .color = core::colors::Title, .align = ui::TextAlign::Left}
			)
			.child_of(panel);
	std::ignore = ui::CreateTextInput(
					  world,
					  {.x = rightX + kPad + 52.0f, .y = colY + 34.0f, .w = 130.0f, .h = 32.0f},
					  std::to_string(seed_),
					  [this](const std::string& text) {
						  try {
							  seed_ = std::stoi(text);
						  }
						  catch (...) {
							  // keep previous seed on invalid input
						  }
					  }
	).child_of(panel);
	std::ignore = ui::CreateButton(
					  world,
					  {.x = rightX + kRightPanelW - kPad - 120.0f, .y = colY + 34.0f, .w = 120.0f, .h = 32.0f},
					  "Regenerate",
					  [this, w = flecs::world{world}](flecs::entity) mutable { Regenerate(w); }
	).child_of(panel);

	// Export report / image + status.
	flecs::entity reportBtn = ui::CreateButton(
								  world,
								  {.x = rightX + kPad, .y = colY + 74.0f, .w = 150.0f, .h = 30.0f},
								  "Export Report",
								  [this, w = flecs::world{world}](flecs::entity) mutable { ExportReport(w); }
	).child_of(panel);
	reportBtn.get_mut<ui::Button>().font_size = 15.0f;
	flecs::entity imageBtn = ui::CreateButton(
								 world,
								 {.x = rightX + kPad + 158.0f, .y = colY + 74.0f, .w = 150.0f, .h = 30.0f},
								 "Export Image",
								 [this, w = flecs::world{world}](flecs::entity) mutable { ExportImage(w); }
	).child_of(panel);
	imageBtn.get_mut<ui::Button>().font_size = 15.0f;
	exportStatus_ =
		ui::CreateLabel(
			world,
			{.x = rightX + kPad, .y = colY + 108.0f, .w = kRightPanelW - kPad * 2, .h = 20.0f},
			"",
			13.0f
		)
			.set<ui::Label>(
				{.text = "", .font_size = 13.0f, .color = core::colors::Subtle, .align = ui::TextAlign::Left}
			)
			.child_of(panel);

	// Prev / Next navigation.
	std::ignore = ui::CreateButton(
					  world,
					  {.x = rightX + kPad, .y = colY + 132.0f, .w = 165.0f, .h = 30.0f},
					  "< Previous",
					  [this](flecs::entity) { SetStep(currentStep_ - 1); }
	).child_of(panel);
	std::ignore = ui::CreateButton(
					  world,
					  {.x = rightX + kRightPanelW - kPad - 165.0f, .y = colY + 132.0f, .w = 165.0f, .h = 30.0f},
					  "Next >",
					  [this](flecs::entity) { SetStep(currentStep_ + 1); }
	).child_of(panel);

	// Scrollable step list fills the rest of the panel.
	const float listY = colY + 170.0f;
	const float listH = colH - (listY - colY) - kPad;
	timelineScroll_ =
		ui::CreateScrollRect(world, {.x = rightX + kPad, .y = listY, .w = kRightPanelW - kPad * 2, .h = listH})
			.child_of(panel);

	std::ignore =
		ui::CreateButton(world, {.x = sw - 110.0f, .y = 12.0f, .w = 92.0f, .h = 30.0f}, "Back", [this](flecs::entity) {
			if (onBackPressed) {
				onBackPressed();
			}
		}).child_of(uiRoot_);

	// Play — commit this previewed world and enter it. The world is already published as the
	// CurrentWorld singleton (see Regenerate); FarmWorldScene builds its tilemap/entities from it.
	std::ignore = ui::CreateButton(
					  world,
					  {.x = sw - 212.0f, .y = 12.0f, .w = 96.0f, .h = 30.0f},
					  "Play",
					  [this](flecs::entity) {
						  if (onPlayPressed) {
							  onPlayPressed();
						  }
					  }
	)
					  .set_name("WorldGenPreviewPlayButton")
					  .child_of(uiRoot_);

	// Left column: the live-tunable generation-parameter sliders (tickets 04–10 knobs).
	BuildParamsPanel(world, {.x = kPad, .y = kTopBar, .w = kParamsPanelW, .h = sh - kTopBar - kPad});
}

void WorldGenPreviewScene::RebuildTimelineList(const flecs::world& world) {
	stepButtons_.clear();
	if (timelineList_) {
		timelineList_.destruct();
		timelineList_ = {};
	}
	if (!timelineScroll_) {
		return;
	}

	const auto& scrollRect = timelineScroll_.get<ui::UIRect>().rect;
	timelineList_ = ui::CreateStack(
						world,
						{.x = scrollRect.x, .y = scrollRect.y, .w = scrollRect.w, .h = 0.0f},
						{.direction = ui::StackDirection::Vertical, .spacing = 4.0f, .padding = 4.0f}
	)
						.child_of(timelineScroll_);

	for (int i = 0; i < static_cast<int>(world_.timeline.steps.size()); ++i) {
		const std::string& label = world_.timeline.steps[static_cast<size_t>(i)].label;
		flecs::entity btn =
			ui::CreateButton(world, {.x = 0.0f, .y = 0.0f, .w = 0.0f, .h = 26.0f}, label, [this, i](flecs::entity) {
				SetStep(i);
			}).child_of(timelineList_);
		ui::Button& b = btn.get_mut<ui::Button>();
		b.font_size = 13.0f; // compact so longer labels fit the panel
		b.normal = core::colors::PanelBg;
		b.text_color = core::colors::Subtle;
		stepButtons_.push_back(btn);
	}
}

void WorldGenPreviewScene::AddParamHeader(const flecs::world& world, flecs::entity list, const std::string& text) {
	std::ignore = ui::CreateLabel(world, {.x = 0.0f, .y = 0.0f, .w = 0.0f, .h = 22.0f}, text, 14.0f)
					  .set<ui::Label>(
						  {.text = text, .font_size = 14.0f, .color = core::colors::Accent, .align = ui::TextAlign::Left}
					  )
					  .child_of(list);
}

void WorldGenPreviewScene::AddFloatKnob(
	const flecs::world& world,
	flecs::entity list,
	std::string name,
	const float value,
	const float min,
	const float max,
	const float step,
	std::function<void(float)> apply
) {
	const std::string initial = name + ": " + FormatFloat(value);
	const flecs::entity label =
		ui::CreateLabel(world, {.x = 0.0f, .y = 0.0f, .w = 0.0f, .h = 16.0f}, initial, 12.0f)
			.set<ui::Label>(
				{.text = initial, .font_size = 12.0f, .color = core::colors::Text, .align = ui::TextAlign::Left}
			)
			.child_of(list);
	std::ignore = ui::CreateSlider(
					  world,
					  {.x = 0.0f, .y = 0.0f, .w = 0.0f, .h = 16.0f},
					  value,
					  min,
					  max,
					  [this, w = flecs::world{world}, label, name = std::move(name), apply = std::move(apply)](
						  const float v
					  ) mutable {
						  apply(v);
						  if (label && label.has<ui::Label>()) {
							  label.get_mut<ui::Label>().text = name + ": " + FormatFloat(v);
						  }
						  Regenerate(w);
					  },
					  step
	).child_of(list);
}

void WorldGenPreviewScene::AddIntKnob(
	const flecs::world& world,
	flecs::entity list,
	std::string name,
	const int value,
	const int min,
	const int max,
	std::function<void(int)> apply
) {
	const std::string initial = name + ": " + std::to_string(value);
	const flecs::entity label =
		ui::CreateLabel(world, {.x = 0.0f, .y = 0.0f, .w = 0.0f, .h = 16.0f}, initial, 12.0f)
			.set<ui::Label>(
				{.text = initial, .font_size = 12.0f, .color = core::colors::Text, .align = ui::TextAlign::Left}
			)
			.child_of(list);
	std::ignore = ui::CreateSlider(
					  world,
					  {.x = 0.0f, .y = 0.0f, .w = 0.0f, .h = 16.0f},
					  static_cast<float>(value),
					  static_cast<float>(min),
					  static_cast<float>(max),
					  [this, w = flecs::world{world}, label, name = std::move(name), apply = std::move(apply)](
						  const float v
					  ) mutable {
						  const int iv = static_cast<int>(std::lround(v));
						  apply(iv);
						  if (label && label.has<ui::Label>()) {
							  label.get_mut<ui::Label>().text = name + ": " + std::to_string(iv);
						  }
						  Regenerate(w);
					  },
					  1.0f
	).child_of(list);
}

// Build the left-hand parameters column: a scrollable stack of labelled sliders, one per generation
// knob introduced across tickets 04–10, grouped under section headers. Each slider mutates params_
// and regenerates live so a developer can tune density, costs, spacing, and population ratios without
// recompiling. These are a developer tuning surface; the player-facing flow just rerolls the seed.
// Regeneration runs at the full play-world size (see CanonicalFarmParams), on demand per edit.
void WorldGenPreviewScene::BuildParamsPanel(const flecs::world& world, const core::Rect& panelRect) {
	const auto panel =
		ui::CreatePanel(world, panelRect, {.color = core::colors::PanelBg, .roundness = 0.03f}).child_of(uiRoot_);

	std::ignore = ui::CreateLabel(
					  world,
					  {.x = panelRect.x + kPad, .y = panelRect.y + 8.0f, .w = panelRect.w - kPad * 2, .h = 24.0f},
					  "Generation Parameters",
					  18.0f
	)
					  .set<ui::Label>(
						  {.text = "Generation Parameters", .font_size = 18.0f, .color = core::colors::Title}
					  )
					  .child_of(panel);
	std::ignore = ui::CreateLabel(
					  world,
					  {.x = panelRect.x + kPad, .y = panelRect.y + 32.0f, .w = panelRect.w - kPad * 2, .h = 18.0f},
					  "Drag a slider to retune + regenerate live",
					  12.0f
	)
					  .set<ui::Label>(
						  {.text = "Drag a slider to retune + regenerate live",
						   .font_size = 12.0f,
						   .color = core::colors::Subtle,
						   .align = ui::TextAlign::Left}
					  )
					  .child_of(panel);

	const float listY = panelRect.y + 56.0f;
	const float listH = panelRect.h - (listY - panelRect.y) - kPad;
	paramsScroll_ =
		ui::CreateScrollRect(world, {.x = panelRect.x + kPad, .y = listY, .w = panelRect.w - kPad * 2, .h = listH})
			.child_of(panel);

	const auto& scrollRect = paramsScroll_.get<ui::UIRect>().rect;
	paramsList_ = ui::CreateStack(
					  world,
					  {.x = scrollRect.x, .y = scrollRect.y, .w = scrollRect.w, .h = 0.0f},
					  {.direction = ui::StackDirection::Vertical, .spacing = 4.0f, .padding = 6.0f}
	)
					  .child_of(paramsScroll_);
	const flecs::entity list = paramsList_;

	// ── Town layout (ticket 04) ──
	AddParamHeader(world, list, "Town layout (04)");
	AddFloatKnob(world, list, "buildingsPerCapita", params_.buildingsPerCapita, 0.01f, 0.30f, 0.0f, [this](const float v) {
		params_.buildingsPerCapita = v;
	});
	AddIntKnob(world, list, "minBuildingsPerTown", params_.minBuildingsPerTown, 0, 10, [this](const int v) {
		params_.minBuildingsPerTown = v;
	});
	AddIntKnob(world, list, "maxBuildingsPerTown", params_.maxBuildingsPerTown, 1, 30, [this](const int v) {
		params_.maxBuildingsPerTown = v;
	});
	AddIntKnob(world, list, "townPlotGap", params_.townPlotGap, 0, 4, [this](const int v) { params_.townPlotGap = v; });
	AddIntKnob(world, list, "townPlazaRadius", params_.townPlazaRadius, 0, 5, [this](const int v) {
		params_.townPlazaRadius = v;
	});

	// ── Forest trees (ticket 05) ──
	AddParamHeader(world, list, "Forest trees (05)");
	AddFloatKnob(world, list, "forestTreeDensity", params_.forestTreeDensity, 0.0f, 1.0f, 0.0f, [this](const float v) {
		params_.forestTreeDensity = v;
	});
	AddFloatKnob(world, list, "deepForestTreeDensity", params_.deepForestTreeDensity, 0.0f, 1.0f, 0.0f, [this](const float v) {
		params_.deepForestTreeDensity = v;
	});
	AddFloatKnob(world, list, "forestClearingThreshold", params_.forestClearingThreshold, 0.0f, 1.0f, 0.0f, [this](const float v) {
		params_.forestClearingThreshold = v;
	});
	AddFloatKnob(world, list, "deepForestClearingThreshold", params_.deepForestClearingThreshold, 0.0f, 1.0f, 0.0f, [this](const float v) {
		params_.deepForestClearingThreshold = v;
	});
	AddFloatKnob(world, list, "forestCoreThreshold", params_.forestCoreThreshold, 0.0f, 1.0f, 0.0f, [this](const float v) {
		params_.forestCoreThreshold = v;
	});
	AddFloatKnob(world, list, "treeDensityNoiseScale", params_.treeDensityNoiseScale, 0.005f, 0.30f, 0.0f, [this](const float v) {
		params_.treeDensityNoiseScale = v;
	});

	// ── Road routing (ticket 06) — passable biome costs + reuse/avoid/redundancy ──
	AddParamHeader(world, list, "Road routing (06)");
	AddFloatKnob(world, list, "roadCost.Beach", params_.roadBiomeCost[static_cast<std::size_t>(Biome::Beach)], 0.1f, 10.0f, 0.0f, [this](const float v) {
		params_.roadBiomeCost[static_cast<std::size_t>(Biome::Beach)] = v;
	});
	AddFloatKnob(world, list, "roadCost.Plains", params_.roadBiomeCost[static_cast<std::size_t>(Biome::Plains)], 0.1f, 10.0f, 0.0f, [this](const float v) {
		params_.roadBiomeCost[static_cast<std::size_t>(Biome::Plains)] = v;
	});
	AddFloatKnob(world, list, "roadCost.Forest", params_.roadBiomeCost[static_cast<std::size_t>(Biome::Forest)], 0.1f, 10.0f, 0.0f, [this](const float v) {
		params_.roadBiomeCost[static_cast<std::size_t>(Biome::Forest)] = v;
	});
	AddFloatKnob(world, list, "roadCost.Hills", params_.roadBiomeCost[static_cast<std::size_t>(Biome::Hills)], 0.1f, 10.0f, 0.0f, [this](const float v) {
		params_.roadBiomeCost[static_cast<std::size_t>(Biome::Hills)] = v;
	});
	AddFloatKnob(world, list, "roadCost.River", params_.roadBiomeCost[static_cast<std::size_t>(Biome::River)], 0.1f, 10.0f, 0.0f, [this](const float v) {
		params_.roadBiomeCost[static_cast<std::size_t>(Biome::River)] = v;
	});
	AddFloatKnob(world, list, "roadExistingDiscount", params_.roadExistingDiscount, 0.05f, 1.0f, 0.0f, [this](const float v) {
		params_.roadExistingDiscount = v;
	});
	AddFloatKnob(world, list, "roadTownAvoidCost", params_.roadTownAvoidCost, 0.0f, 100.0f, 0.0f, [this](const float v) {
		params_.roadTownAvoidCost = v;
	});
	AddFloatKnob(world, list, "roadRedundancyDetour", params_.roadRedundancyDetour, 1.0f, 3.0f, 0.0f, [this](const float v) {
		params_.roadRedundancyDetour = v;
	});

	// ── Road hubs + crossroads outposts (ticket 07) ──
	AddParamHeader(world, list, "Hubs + outposts (07)");
	AddIntKnob(world, list, "hubCount", params_.hubCount, 0, 16, [this](const int v) { params_.hubCount = v; });
	AddIntKnob(world, list, "hubMinSpacing", params_.hubMinSpacing, 4, 200, [this](const int v) {
		params_.hubMinSpacing = v;
	});
	AddFloatKnob(world, list, "hubRouteDiscount", params_.hubRouteDiscount, 0.1f, 1.0f, 0.0f, [this](const float v) {
		params_.hubRouteDiscount = v;
	});
	AddIntKnob(world, list, "hubDiscountRadius", params_.hubDiscountRadius, 1, 100, [this](const int v) {
		params_.hubDiscountRadius = v;
	});
	AddFloatKnob(world, list, "townAttractorDiscount", params_.townAttractorDiscount, 0.1f, 1.0f, 0.0f, [this](const float v) {
		params_.townAttractorDiscount = v;
	});
	AddIntKnob(world, list, "townAttractorRadius", params_.townAttractorRadius, 1, 150, [this](const int v) {
		params_.townAttractorRadius = v;
	});
	AddFloatKnob(world, list, "townAttractorConnectivity", params_.townAttractorConnectivity, 0.0f, 1.0f, 0.0f, [this](const float v) {
		params_.townAttractorConnectivity = v;
	});
	AddIntKnob(world, list, "hubMarkerRadius", params_.hubMarkerRadius, 0, 5, [this](const int v) {
		params_.hubMarkerRadius = v;
	});
	AddIntKnob(world, list, "outpostConvergenceRadius", params_.outpostConvergenceRadius, 1, 80, [this](const int v) {
		params_.outpostConvergenceRadius = v;
	});
	AddIntKnob(world, list, "outpostMinConvergence", params_.outpostMinConvergence, 1, 8, [this](const int v) {
		params_.outpostMinConvergence = v;
	});
	AddIntKnob(world, list, "outpostMinTownDistance", params_.outpostMinTownDistance, 1, 150, [this](const int v) {
		params_.outpostMinTownDistance = v;
	});
	AddIntKnob(world, list, "maxCrossroadsOutposts", params_.maxCrossroadsOutposts, 0, 5, [this](const int v) {
		params_.maxCrossroadsOutposts = v;
	});
	AddIntKnob(world, list, "outpostPopulation", params_.outpostPopulation, 1, 100, [this](const int v) {
		params_.outpostPopulation = v;
	});

	// ── Bridges (ticket 08) ──
	AddParamHeader(world, list, "Bridges (08)");
	AddIntKnob(world, list, "maxBridgeSpan", params_.maxBridgeSpan, 1, 30, [this](const int v) {
		params_.maxBridgeSpan = v;
	});

	// ── Wandering creatures (ticket 10) ──
	AddParamHeader(world, list, "Creatures (10)");
	AddIntKnob(world, list, "creatureTownRadius", params_.creatureTownRadius, 1, 30, [this](const int v) {
		params_.creatureTownRadius = v;
	});
	AddIntKnob(world, list, "minLivestockPerTown", params_.minLivestockPerTown, 0, 10, [this](const int v) {
		params_.minLivestockPerTown = v;
	});
	AddIntKnob(world, list, "maxLivestockPerTown", params_.maxLivestockPerTown, 0, 15, [this](const int v) {
		params_.maxLivestockPerTown = v;
	});
	AddIntKnob(world, list, "creatureAreaSize", params_.creatureAreaSize, 16, 256, [this](const int v) {
		params_.creatureAreaSize = v;
	});
	AddIntKnob(world, list, "minWildlifePerArea", params_.minWildlifePerArea, 0, 8, [this](const int v) {
		params_.minWildlifePerArea = v;
	});
	AddIntKnob(world, list, "maxWildlifePerArea", params_.maxWildlifePerArea, 0, 10, [this](const int v) {
		params_.maxWildlifePerArea = v;
	});
}

void WorldGenPreviewScene::HandleInput(const flecs::world&, const input::InputState& input) {
	const float mx = input.mouse.window_position.x;
	const float my = input.mouse.window_position.y;
	const bool insideMap =
		mx >= mapRect_.x && my >= mapRect_.y && mx <= mapRect_.x + mapRect_.w && my <= mapRect_.y + mapRect_.h;
	if (!insideMap) {
		return;
	}

	if (const float wheel = input.mouse.scroll.y; wheel != 0.0f) {
		zoom_ = std::clamp(zoom_ * (1.0f + wheel * 0.12f), kMinZoom, kMaxZoom);
	}
	if (input.mouse.left.state) {
		panCentreCell_.x -= input.mouse.relative.x / zoom_;
		panCentreCell_.y -= input.mouse.relative.y / zoom_;
		panCentreCell_.x = std::clamp(panCentreCell_.x, 0.0f, static_cast<float>(world_.grid.width));
		panCentreCell_.y = std::clamp(panCentreCell_.y, 0.0f, static_cast<float>(world_.grid.height));
	}
}

void WorldGenPreviewScene::DrawMap(platform::Platform& platform) const {
	platform.BeginScissor(mapRect_);
	platform.DrawRect(mapRect_, {.r = 12, .g = 14, .b = 18, .a = 255});

	const bool showWater = currentStep_ >= waterStepIndex_;
	const bool showRivers = currentStep_ >= riverStepIndex_;

	const Grid& grid = world_.grid;
	const float halfW = mapRect_.w * 0.5f;
	const float halfH = mapRect_.h * 0.5f;
	const int minX = std::max(0, static_cast<int>(std::floor(panCentreCell_.x - halfW / zoom_)) - 1);
	const int maxX = std::min(grid.width - 1, static_cast<int>(std::ceil(panCentreCell_.x + halfW / zoom_)) + 1);
	const int minY = std::max(0, static_cast<int>(std::floor(panCentreCell_.y - halfH / zoom_)) - 1);
	const int maxY = std::min(grid.height - 1, static_cast<int>(std::ceil(panCentreCell_.y + halfH / zoom_)) + 1);
	const float cellPx = std::ceil(zoom_) + 1.0f;

	for (int y = minY; y <= maxY; ++y) {
		for (int x = minX; x <= maxX; ++x) {
			Biome b = grid.BiomeAt(x, y);
			if ((b == Biome::Ocean || b == Biome::Lake) && !showWater) {
				b = LandBand(grid, x, y);
			}
			else if (b == Biome::River && !showRivers) {
				b = LandBand(grid, x, y);
			}
			const glm::vec2 s = CellToScreen({static_cast<float>(x), static_cast<float>(y)});
			platform.DrawRect({.x = s.x, .y = s.y, .w = cellPx, .h = cellPx}, BiomeColour(b));
		}
	}

	// Cumulative towns + present roads up to the selected step.
	std::unordered_map<int, TownStatus> revealed;
	std::set<std::pair<int, int>> presentRoads;
	for (int i = 0; i <= currentStep_ && i < static_cast<int>(world_.timeline.steps.size()); ++i) {
		const WorldSnapshot& step = world_.timeline.steps[static_cast<size_t>(i)];
		if (step.kind == SnapshotKind::TownFounded) {
			for (const int id : step.addedTownIds) {
				revealed[id] = TownStatus::Living;
			}
		}
		else if (step.kind == SnapshotKind::TownCollapsed) {
			for (const int id : step.addedTownIds) {
				revealed[id] = TownStatus::Ruined;
			}
		}
		for (const auto& road : step.addedRoads) {
			presentRoads.insert(road);
		}
		for (const auto& road : step.removedRoads) {
			presentRoads.erase(road);
		}
	}

	// tilePx: screen pixels per grid tile, at minimum 2 so even at fit-to-screen zoom shapes are
	// drawn as distinct filled rects and not invisible sub-pixel specks.
	const float tilePx = std::max(2.0f, std::ceil(zoom_)) + 1.0f;

	// Draw revealed town layouts (floor tiles + building footprints) before roads so road tiles
	// naturally overlay street-level ground. This gives each town a recognisable cluster shape
	// instead of just a dot, even at fit-to-screen zoom.
	constexpr core::Rgba kFloorLiving = {.r = 158, .g = 148, .b = 130, .a = 255}; // stone floor
	constexpr core::Rgba kFloorRuined = {.r = 95, .g = 82, .b = 65, .a = 255};   // darkened rubble
	constexpr core::Rgba kBuildLiving = {.r = 90, .g = 78, .b = 60, .a = 255};   // wall / roof
	constexpr core::Rgba kBuildRuined = {.r = 60, .g = 52, .b = 40, .a = 255};   // ruined wall

	for (const auto& layout : world_.townLayouts) {
		const auto rIt = revealed.find(layout.townId);
		if (rIt == revealed.end()) continue;
		const bool ruined = rIt->second == TownStatus::Ruined;
		const core::Rgba floorCol = ruined ? kFloorRuined : kFloorLiving;
		const core::Rgba buildCol = ruined ? kBuildRuined : kBuildLiving;
		for (const glm::ivec2 tile : layout.floorTiles) {
			const glm::vec2 s = CellToScreen({static_cast<float>(tile.x), static_cast<float>(tile.y)});
			platform.DrawRect({.x = s.x, .y = s.y, .w = tilePx, .h = tilePx}, floorCol);
		}
		for (const TownBuilding& bldg : layout.buildings) {
			for (const glm::ivec2 fp : bldg.footprint) {
				const glm::vec2 s = CellToScreen({static_cast<float>(fp.x), static_cast<float>(fp.y)});
				platform.DrawRect({.x = s.x, .y = s.y, .w = tilePx, .h = tilePx}, buildCol);
			}
		}
	}

	// Index road segments by their (min,max) town-id pair for O(1) lookup below. Segments and
	// timeline pairs are both normalised min-first so keys always agree.
	std::unordered_map<int, const RoadSegment*> roadByKey;
	roadByKey.reserve(world_.roads.segments.size());
	for (const RoadSegment& seg : world_.roads.segments) {
		roadByKey[seg.townA * 100000 + seg.townB] = &seg;
	}

	// Draw roads as their actual A*-routed tile paths so the preview matches the in-game geometry.
	constexpr core::Rgba kRoadColor = {.r = 70, .g = 50, .b = 36, .a = 255};
	for (const auto& [a, b] : presentRoads) {
		const int key = std::min(a, b) * 100000 + std::max(a, b);
		const auto it = roadByKey.find(key);
		if (it != roadByKey.end() && !it->second->path.empty()) {
			for (const glm::ivec2 tile : it->second->path) {
				const glm::vec2 s = CellToScreen({static_cast<float>(tile.x), static_cast<float>(tile.y)});
				platform.DrawRect({.x = s.x, .y = s.y, .w = tilePx, .h = tilePx}, kRoadColor);
			}
		}
		// Silently skip connections with no routed path (blocked terrain); no fallback line.
	}

	// Pins scale with zoom: small at overview, large enough to distinguish at high zoom.
	const float pinR = std::max(2.5f, zoom_ * 1.8f);
	for (const auto& [id, status] : revealed) {
		const TownSite* town = world_.FindTown(id);
		if (town == nullptr) {
			continue;
		}
		const glm::vec2 s = CellToScreen({town->location.x + 0.5f, town->location.y + 0.5f});
		if (status == TownStatus::Living) {
			platform.DrawCircle(s, pinR + 1.0f, core::colors::White);
			platform.DrawCircle(s, pinR, {.r = 90, .g = 210, .b = 90, .a = 255});
		}
		else {
			platform.DrawCircle(s, pinR + 1.0f, {.r = 40, .g = 40, .b = 40, .a = 255});
			platform.DrawCircle(s, pinR, {.r = 170, .g = 70, .b = 70, .a = 255});
		}
		// Town name label (shown from the default fit-zoom and in; hidden only when zoomed far out).
		if (zoom_ >= 3.0f) {
			const float tx = s.x + pinR + 3.0f;
			const float ty = s.y - 8.0f;
			platform.DrawText(town->name, tx + 1.0f, ty + 1.0f, 15.0f, {.r = 10, .g = 12, .b = 16, .a = 255});
			platform.DrawText(town->name, tx, ty, 15.0f, core::colors::White);
		}
	}

	// Named bodies of water (oceans, lakes, sizeable ponds), drawn at their centroid.
	if (showWater && zoom_ >= 3.0f) {
		for (const WaterBody& body : world_.waterBodies) {
			if (body.name.empty()) {
				continue;
			}
			const glm::vec2 c = CellToScreen({body.centroid.x + 0.5f, body.centroid.y + 0.5f});
			const float halfText = platform.MeasureText(body.name, 14.0f) * 0.5f;
			platform
				.DrawText(body.name, c.x - halfText + 1.0f, c.y + 1.0f, 14.0f, {.r = 8, .g = 10, .b = 14, .a = 255});
			platform.DrawText(body.name, c.x - halfText, c.y, 14.0f, {.r = 205, .g = 225, .b = 245, .a = 255});
		}
	}

	platform.EndScissor();
	platform.DrawRectLines(mapRect_, 1.0f, core::colors::Border);
}

void WorldGenPreviewScene::DrawUI(const flecs::world& world, const input::InputState&) {
	auto* platform = world.get<platform::PlatformRef>().ptr;
	if (platform != nullptr) {
		DrawMap(*platform);
	}
}

} // namespace game::farm
