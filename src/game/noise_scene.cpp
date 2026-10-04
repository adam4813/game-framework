#include "noise_scene.hpp"

#include <cmath>
#include <format>
#include <string>
#include <vector>

#include "engine/engine.hpp"

using namespace engine;

namespace game {

namespace {
constexpr int kTexSize = 256;
constexpr float kControlW = 340.0F;
constexpr float kPad = 16.0F;
} // namespace

void NoiseScene::InitializePipeline(const flecs::world& world) {
	// clang-format off
	pipeline_ = world.pipeline()
					.with(flecs::System)
					.without<scene::GameScene>()
					.with(flecs::Phase).cascade(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::DependsOn)
					.without(flecs::Disabled).up(flecs::ChildOf)
					.build();
	// clang-format on
}

void NoiseScene::Load(flecs::world& world) {
	dirty_ = true;

	world.defer_suspend();
	BuildUI(world);
	world.defer_resume();
}

void NoiseScene::Tick(const flecs::world& world, float) {
	const auto& config = world.get<noise::NoiseConfig>();
	// Detect changes from scripts (Noise_SetAlgorithm, Noise_SetSeed globals).
	if (config.active_algorithm != algorithm_) {
		algorithm_ = config.active_algorithm;
		dirty_ = true;
	}
	if (config.seed != seed_) {
		seed_ = config.seed;
		dirty_ = true;
		// Sync the seed slider + value label so the UI reflects the script-driven change.
		if (seedSlider_) {
			seedSlider_.get_mut<ui::Slider>().value = static_cast<float>(seed_);
		}
		if (seedValueLabel_) {
			seedValueLabel_.get_mut<ui::Label>().text = std::to_string(seed_);
		}
	}
	if (dirty_) {
		RebuildTexture(world);
		dirty_ = false;
	}
}

void NoiseScene::Unload(flecs::world& world) {
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = {};
	}
	seedSlider_ = {};
	seedValueLabel_ = {};
	imagePanel_ = {};

	if (noiseTexture_ >= 0) {
		world.get<platform::PlatformRef>().ptr->UnloadTexture(noiseTexture_);
		noiseTexture_ = -1;
	}
}

void NoiseScene::RebuildTexture(const flecs::world& world) {
	const auto& registry = world.get<noise::NoiseRegistry>();

	// Offset by a seed-derived amount to move well away from the Perlin origin.
	// At (0,0), all gradients are zero — sampling there produces flat/dark noise.
	const glm::vec2 offset{static_cast<float>(seed_) * 1.7f + 100.0f, static_cast<float>(seed_) * 2.3f + 200.0f};
	const auto map =
		noise::GenerateHeightMap(registry.GetSampler(algorithm_, seed_), kTexSize, kTexSize, offset, scale_, params_);

	std::vector<uint8_t> rgba(kTexSize * kTexSize * 4);
	for (int i = 0; i < kTexSize * kTexSize; ++i) {
		const auto v = static_cast<uint8_t>(map[static_cast<size_t>(i)] * 255.0F);
		rgba[static_cast<size_t>(i) * 4 + 0] = v;
		rgba[static_cast<size_t>(i) * 4 + 1] = v;
		rgba[static_cast<size_t>(i) * 4 + 2] = v;
		rgba[static_cast<size_t>(i) * 4 + 3] = 255;
	}

	auto* platform = world.get<platform::PlatformRef>().ptr;
	if (noiseTexture_ < 0) {
		noiseTexture_ = platform->UploadTexture(kTexSize, kTexSize, rgba);
		platform->SetTextureWrap(noiseTexture_, true); // clamp edges
	}
	else {
		platform->UpdateTexture(noiseTexture_, rgba);
	}

	if (imagePanel_) {
		imagePanel_.get_mut<ui::Panel>().texture = noiseTexture_;
	}
}

void NoiseScene::BuildUI(const flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto sw = static_cast<float>(platform->Width());
	const auto sh = static_cast<float>(platform->Height());
	const std::string assetDir = platform->AssetDirectory();

	uiRoot_ = world.entity("NoiseSceneUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = sw, .h = sh}});

	// ── Header ──
	std::ignore = ui::CreateLabel(world, {.x = kPad, .y = 10.0F, .w = 400.0F, .h = 36.0F}, "Noise Visualizer", 28.0F)
					  .set<ui::Label>({.text = "Noise Visualizer", .font_size = 28.0F, .color = core::colors::Title})
					  .child_of(uiRoot_);

	std::ignore =
		ui::CreateButton(world, {.x = sw - 120.0F, .y = 8.0F, .w = 100.0F, .h = 38.0F}, "Back", [this](flecs::entity) {
			if (onBackPressed) onBackPressed();
		}).child_of(uiRoot_);

	// ── Algorithm buttons ──
	// "Perlin" is script-driven (demonstrates the script button binding pattern).
	// Unavailable algorithms are greyed out with no callback.
	constexpr float algo_y = 56.0F;
	struct AlgoEntry {
		std::string key;
		std::string label;
		bool available;
	};
	const std::vector<AlgoEntry> algos = {
		{.key = "perlin", .label = "Perlin", .available = true},
		{.key = "simplex", .label = "Simplex", .available = true},
		{.key = "value", .label = "Value", .available = true},
		{.key = "worley_f1", .label = "Worley F1", .available = true},
		{.key = "worley_f2", .label = "Worley F2", .available = true},
		{.key = "worley_f2_f1", .label = "Worley F2-F1", .available = true},
	};
	float ax = kPad;
	for (const auto& [key, label, available] : algos) {
		const float bw = platform->MeasureText(label, 18.0F) + 24.0F;
		const bool active = (key == algorithm_);
		const std::string btn_name = "NoiseAlgo_" + key;

		auto btn = ui::CreateButton(world, {.x = ax, .y = algo_y, .w = bw, .h = 34.0F}, label, nullptr)
					   .set_name(btn_name.c_str())
					   .child_of(uiRoot_);

		auto& btn_comp = btn.get_mut<ui::Button>();
		if (!available) {
			btn_comp.normal = core::colors::PanelBg;
			btn_comp.text_color = core::colors::Subtle;
		}
		else if (active) {
			btn_comp.normal = core::colors::Title;
			btn_comp.text_color = core::colors::Background;
		}

		if (available) {
			const std::string script_name = "NoiseAlgoScript_" + key;
			world.entity(script_name.c_str())
				.child_of(btn)
				.set<scripting::ScriptComponent>({.source_path = assetDir + "/scripts/noise_algo_" + key + ".as"});
		}
		ax += bw + 8.0F;
	}

	// ── Noise image panel ──
	constexpr float img_y = algo_y + 42.0F;
	imagePanel_ = ui::CreatePanel(
					  world,
					  {.x = kPad, .y = img_y, .w = static_cast<float>(kTexSize), .h = static_cast<float>(kTexSize)},
					  {.color = core::colors::White, .border_color = core::colors::Border, .border_thickness = 1.0F}
	)
					  .child_of(uiRoot_);

	// ── Controls panel (right column) ──
	const float ctrl_x = sw - kControlW - kPad;
	constexpr float ctrl_y = algo_y;
	const float ctrl_h = sh - ctrl_y - kPad;
	const auto ctrl_panel = ui::CreatePanel(
								world,
								{.x = ctrl_x, .y = ctrl_y, .w = kControlW, .h = ctrl_h},
								{.color = core::colors::PanelBg, .roundness = 0.04F}
	)
								.child_of(uiRoot_);

	// Helper: one labelled slider row (label + value readout + slider).
	// Labels and sliders are parented to ctrl_panel so they're drawn on top of it
	// (Flecs children() iteration order is archetype-based, not creation-based, so
	// siblings in different archetypes may emit in any order).
	float ry = ctrl_y + kPad;
	constexpr float row_h = 58.0F;
	constexpr auto slider_w = kControlW - kPad * 2.0F;

	const auto add_row = [&](const std::string_view name,
							 float init_val,
							 const float min_v,
							 const float max_v,
							 const float step_v,
							 const std::string_view fmt_str,
							 std::function<void(float)> on_change) {
		const std::string fmt{fmt_str};
		const std::string init_text = std::vformat(fmt, std::make_format_args(init_val));
		std::ignore =
			ui::CreateLabel(world, {.x = ctrl_x + kPad, .y = ry, .w = 140.0F, .h = 22.0F}, std::string{name}, 15.0F)
				.set<ui::Label>(
					{.text = std::string{name},
					 .font_size = 15.0F,
					 .color = core::colors::Title,
					 .align = ui::TextAlign::Left}
				)
				.child_of(ctrl_panel);

		const auto val_lbl =
			ui::CreateLabel(
				world,
				{.x = ctrl_x + kControlW - kPad - 60.0F, .y = ry, .w = 60.0F, .h = 22.0F},
				init_text,
				15.0F
			)
				.set<ui::Label>(
					{.text = init_text, .font_size = 15.0F, .color = core::colors::Text, .align = ui::TextAlign::Right}
				)
				.child_of(ctrl_panel);

		std::ignore = ui::CreateSlider(
						  world,
						  {.x = ctrl_x + kPad, .y = ry + 24.0F, .w = slider_w, .h = 26.0F},
						  init_val,
						  min_v,
						  max_v,
						  [val_lbl, fmt, on_change = std::move(on_change)](float v) mutable {
							  val_lbl.get_mut<ui::Label>().text = std::vformat(fmt, std::make_format_args(v));
							  on_change(v);
						  },
						  step_v
		)
						  .child_of(ctrl_panel);

		ry += row_h;
	};

	add_row("Scale", scale_, 0.002F, 0.1F, 0.0F, "{:.3f}", [this](const float v) {
		scale_ = v;
		dirty_ = true;
	});

	add_row("Octaves", static_cast<float>(params_.octaves), 1.0F, 8.0F, 1.0F, "{:.0f}", [this](const float v) {
		params_.octaves = static_cast<int>(std::round(v));
		dirty_ = true;
	});

	add_row("Persistence", params_.persistence, 0.1F, 1.0F, 0.0F, "{:.3f}", [this](const float v) {
		params_.persistence = v;
		dirty_ = true;
	});

	add_row("Lacunarity", params_.lacunarity, 1.0F, 4.0F, 0.0F, "{:.3f}", [this](const float v) {
		params_.lacunarity = v;
		dirty_ = true;
	});

	// Seed — integer slider (0–9999). Stored as a member so Tick can update it when scripts
	// change the seed via Noise_SetSeed. Also writes NoiseConfig.seed for script sync.
	// TODO: add a UITextInput component for direct seed entry (platform TextInput() is available
	// but no text-input widget exists yet).
	{
		const auto seed_float = static_cast<float>(seed_);
		const std::string init_text = std::vformat("{:.0f}", std::make_format_args(seed_float));
		std::ignore =
			ui::CreateLabel(world, {.x = ctrl_x + kPad, .y = ry, .w = 140.0F, .h = 22.0F}, "Seed", 15.0F)
				.set<ui::Label>(
					{.text = "Seed", .font_size = 15.0F, .color = core::colors::Title, .align = ui::TextAlign::Left}
				)
				.child_of(ctrl_panel);

		seedValueLabel_ =
			ui::CreateLabel(
				world,
				{.x = ctrl_x + kControlW - kPad - 60.0F, .y = ry, .w = 60.0F, .h = 22.0F},
				init_text,
				15.0F
			)
				.set<ui::Label>(
					{.text = init_text, .font_size = 15.0F, .color = core::colors::Text, .align = ui::TextAlign::Right}
				)
				.child_of(ctrl_panel);

		seedSlider_ = ui::CreateSlider(
						  world,
						  {.x = ctrl_x + kPad, .y = ry + 24.0F, .w = slider_w, .h = 26.0F},
						  static_cast<float>(seed_),
						  0.0F,
						  9999.0F,
						  [this, w = flecs::world{world}](const float v) mutable {
							  seed_ = static_cast<int>(std::round(v));
							  w.get_mut<noise::NoiseConfig>().seed = seed_;
							  dirty_ = true;
						  },
						  1.0F
		)
						  .child_of(ctrl_panel);
	}
}

} // namespace game
