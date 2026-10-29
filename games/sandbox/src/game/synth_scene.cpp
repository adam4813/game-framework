#include "synth_scene.hpp"

#include <format>
#include <string>

#include "engine/engine.hpp"

using namespace engine;

namespace game {

namespace {
constexpr float kPad = 16.0F;
constexpr float kControlW = 420.0F;
constexpr float kRowH = 58.0F;
} // namespace

void SynthScene::InitializePipeline(const flecs::world& world) {
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

void SynthScene::Load(flecs::world& world) {
	voice_ = std::make_unique<SynthDemoVoice>();
	world.get<platform::PlatformRef>().ptr->StartSynth(voice_.get());
	BuildUI(world);
}

void SynthScene::Tick(const flecs::world& world, float) {
	if (world.get<input::InputState>().WasKeyPressed(input::KeyCode::Space)) {
		voice_->click.Fire();
	}
}

void SynthScene::Unload(flecs::world& world) {
	// Stop the stream before freeing the voice it renders.
	world.get<platform::PlatformRef>().ptr->StopSynth();
	voice_.reset();
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = {};
	}
}

void SynthScene::BuildUI(const flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto sw = static_cast<float>(platform->Width());
	const auto sh = static_cast<float>(platform->Height());

	uiRoot_ = world.entity("SynthSceneUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0F, .y = 0.0F, .w = sw, .h = sh}});

	std::ignore = ui::CreateLabel(world, {.x = kPad, .y = 10.0F, .w = 400.0F, .h = 36.0F}, "Synth Demo", 28.0F)
					  .set<ui::Label>({.text = "Synth Demo", .font_size = 28.0F, .color = core::colors::Title})
					  .child_of(uiRoot_);

	std::ignore =
		ui::CreateButton(world, {.x = sw - 120.0F, .y = 8.0F, .w = 100.0F, .h = 38.0F}, "Back", [this](flecs::entity) {
			if (onBackPressed) {
				onBackPressed();
			}
		}).child_of(uiRoot_);

	const float x = (sw - kControlW) / 2.0F;
	float y = 90.0F;

	const auto rowText = [](const std::string& name, const float value) {
		return std::format("{}: {:.2f}", name, value);
	};
	const auto addRow = [&](const std::string& name,
							const float minV,
							const float maxV,
							engine::audio::SynthParam& param) {
		const float init = param.Target();
		const std::string initText = rowText(name, init);
		const auto label =
			ui::CreateLabel(world, {.x = x, .y = y, .w = kControlW, .h = 22.0F}, initText, 16.0F)
				.set<ui::Label>(
					{.text = initText, .font_size = 16.0F, .color = core::colors::Title, .align = ui::TextAlign::Left}
				)
				.child_of(uiRoot_);
		std::ignore = ui::CreateSlider(
						  world,
						  {.x = x, .y = y + 24.0F, .w = kControlW, .h = 26.0F},
						  init,
						  minV,
						  maxV,
						  [label, name, rowText, &param](const float v) {
							  label.get_mut<ui::Label>().text = rowText(name, v);
							  param.Set(v);
						  }
		).child_of(uiRoot_);
		y += kRowH;
	};

	addRow("Whistle Hz", 200.0F, 2000.0F, voice_->whistleHz);
	addRow("Whistle level", 0.0F, 0.5F, voice_->whistleLevel);
	addRow("Hiss level", 0.0F, 1.0F, voice_->hissLevel);

	std::ignore = ui::CreateButton(
					  world,
					  {.x = x, .y = y + 10.0F, .w = 200.0F, .h = 44.0F},
					  "Click (Space)",
					  [this](flecs::entity) { voice_->click.Fire(); }
	)
					  .set_name("SynthClickButton")
					  .child_of(uiRoot_);

	const std::string hint = "Web: audio starts on your first click or key press.";
	std::ignore = ui::CreateLabel(world, {.x = x, .y = y + 70.0F, .w = kControlW, .h = 20.0F}, hint, 14.0F)
					  .set<ui::Label>({.text = hint, .font_size = 14.0F, .color = core::colors::Subtle})
					  .child_of(uiRoot_);
}

} // namespace game
