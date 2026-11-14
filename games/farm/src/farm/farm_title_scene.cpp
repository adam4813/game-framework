#include "farm_title_scene.hpp"

#include <string>
#include <tuple>

#include "constants.hpp"
#include "engine/engine.hpp"
#include "farm_save.hpp"
#include "farm_world_scene.hpp"
#include "worldgen_preview_scene.hpp"

using namespace engine;

namespace game::farm {
namespace {

constexpr float kModalW = 640.0f;
constexpr float kRowH = 56.0f;
constexpr float kModalH = 90.0f + kRowH * static_cast<float>(kFarmSlotCount) + 80.0f;

// Bind the active session to `slot`. For a new game (load = false) the world identity is reset so
// the preview generates fresh; for load/continue (load = true) the scene restores `slot` from disk.
void BindSession(const flecs::world& world, const int slot, const bool load) {
	auto& s = world.get_mut<FarmSession>();
	s.slot = slot;
	s.load_requested = load;
	if (!load) {
		s.seed = 0;
		s.playerX = -1;
		s.playerZ = -1;
	}
}

} // namespace

void FarmTitleScene::InitializePipeline(const flecs::world& world) {
	pipeline_ = ecs::FramePipeline(world).without<scene::GameScene>().build();
	fixedStepPipeline_ = ecs::FixedStepPipeline(world).without<scene::GameScene>().build();
}

void FarmTitleScene::Load(flecs::world& world) {
	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const auto w = static_cast<float>(platform->Width());
	const auto h = static_cast<float>(platform->Height());

	uiRoot_ = world.entity("FarmTitleUI");
	uiRoot_.set<ui::UIRect>({{.x = 0.0f, .y = 0.0f, .w = w, .h = h}});

	std::ignore = ui::CreateLabel(
					  world, {.x = 0.0f, .y = h / 2.0f - 200.0f, .w = w, .h = 72.0f}, "Farm", 64.0f
	)
					  .set<ui::Label>({.text = "Farm", .font_size = 64.0f, .color = core::colors::Title})
					  .child_of(uiRoot_);

	// New Game — pick a slot, then preview the world before playing.
	std::ignore = ui::CreateButton(
					  world,
					  {.x = w / 2.0f - 110.0f, .y = h / 2.0f - 20.0f, .w = 220.0f, .h = 52.0f},
					  "New Game",
					  [this](const flecs::entity& e) { OpenSlotModal(e.world(), SlotMode::New); }
	)
					  .set_name("NewGameButton")
					  .child_of(uiRoot_);

	// Load Game — pick a saved slot to load or clear.
	std::ignore = ui::CreateButton(
					  world,
					  {.x = w / 2.0f - 110.0f, .y = h / 2.0f + 50.0f, .w = 220.0f, .h = 52.0f},
					  "Load Game",
					  [this](const flecs::entity& e) { OpenSlotModal(e.world(), SlotMode::Load); }
	)
					  .set_name("LoadGameButton")
					  .child_of(uiRoot_);

	// Slot-picker modal (shared by New and Load; rows are rebuilt per mode when opened).
	const float mx = w / 2.0f - kModalW / 2.0f;
	const float my = h / 2.0f - kModalH / 2.0f;
	slotModal_ = ui::CreateModal(world, {.x = mx, .y = my, .w = kModalW, .h = kModalH}, {.open = false}).child_of(uiRoot_);

	slotModalTitle_ = ui::CreateLabel(world, {.x = mx, .y = my + 16.0f, .w = kModalW, .h = 34.0f}, "", 26.0f)
						  .set<ui::Label>({.text = "", .font_size = 26.0f, .color = core::colors::Title})
						  .child_of(slotModal_);

	std::ignore = ui::CreateButton(
					  world,
					  {.x = mx + kModalW / 2.0f - 70.0f, .y = my + kModalH - 56.0f, .w = 140.0f, .h = 40.0f},
					  "Cancel",
					  [this](const flecs::entity&) { CloseSlotModal(); }
	).child_of(slotModal_);

	RefreshContinue(world);
}

void FarmTitleScene::Unload(flecs::world&) {
	if (uiRoot_) {
		uiRoot_.destruct();
		uiRoot_ = {};
	}
	continueButton_ = {};
	slotModal_ = {};
	slotModalTitle_ = {};
	slotRowEntities_.clear();
}

void FarmTitleScene::RefreshContinue(const flecs::world& world) {
	const bool has = ReadLastPlayedSlot(world) >= 0;
	if (has && !continueButton_.is_valid()) {
		const auto* platform = world.get<platform::PlatformRef>().ptr;
		const auto w = static_cast<float>(platform->Width());
		const auto h = static_cast<float>(platform->Height());
		continueButton_ = ui::CreateButton(
							   world,
							   {.x = w / 2.0f - 110.0f, .y = h / 2.0f - 90.0f, .w = 220.0f, .h = 52.0f},
							   "Continue",
							   [](const flecs::entity& e) {
								   const flecs::world wr = e.world();
								   const int slot = ReadLastPlayedSlot(wr);
								   if (slot < 0) return;
								   BindSession(wr, slot, /*load=*/true);
								   scene::ActivateScene<FarmWorldSceneTag>(wr);
							   }
		);
		continueButton_.set_name("ContinueButton");
		continueButton_.child_of(uiRoot_);
	}
	else if (!has && continueButton_.is_valid()) {
		continueButton_.destruct();
		continueButton_ = {};
	}
}

void FarmTitleScene::OpenSlotModal(const flecs::world& world, const SlotMode mode) {
	slotMode_ = mode;
	if (slotModalTitle_ && slotModalTitle_.has<ui::Label>()) {
		slotModalTitle_.get_mut<ui::Label>().text = mode == SlotMode::New ? "New Game — Select Slot" : "Load Game — Select Slot";
	}
	RebuildSlotRows(world);
	if (slotModal_ && slotModal_.has<ui::Modal>()) slotModal_.get_mut<ui::Modal>().open = true;
}

void FarmTitleScene::CloseSlotModal() {
	if (slotModal_ && slotModal_.has<ui::Modal>()) slotModal_.get_mut<ui::Modal>().open = false;
}

void FarmTitleScene::RebuildSlotRows(const flecs::world& world) {
	for (const flecs::entity e : slotRowEntities_) {
		if (e.is_valid()) e.destruct();
	}
	slotRowEntities_.clear();

	const auto* platform = world.get<platform::PlatformRef>().ptr;
	const float w = static_cast<float>(platform->Width());
	const float h = static_cast<float>(platform->Height());
	const float mx = w / 2.0f - kModalW / 2.0f;
	const float my = h / 2.0f - kModalH / 2.0f;

	for (int i = 0; i < kFarmSlotCount; ++i) {
		const bool occupied = FarmSlotOccupied(world, i);
		const float rowY = my + 70.0f + static_cast<float>(i) * kRowH;
		const std::string status = "Slot " + std::to_string(i + 1) + (occupied ? ": Saved" : ": Empty");

		const auto statusLabel =
			ui::CreateLabel(world, {.x = mx + 24.0f, .y = rowY, .w = 220.0f, .h = 44.0f}, status, 20.0f)
				.set<ui::Label>(
					{.text = status, .font_size = 20.0f, .color = core::colors::Text, .align = ui::TextAlign::Left}
				);
		statusLabel.child_of(slotModal_);
		slotRowEntities_.push_back(statusLabel);

		if (slotMode_ == SlotMode::New) {
			const char* label = occupied ? "Overwrite" : "Select";
			const auto btn = ui::CreateButton(
								 world,
								 {.x = mx + 256.0f, .y = rowY, .w = 360.0f, .h = 44.0f},
								 label,
								 [this, i](const flecs::entity& e) {
									 const flecs::world wr = e.world();
									 BindSession(wr, i, /*load=*/false);
									 CloseSlotModal();
									 scene::ActivateScene<WorldGenPreviewSceneTag>(wr);
								 }
			);
			btn.set_name(("SlotSelect" + std::to_string(i + 1)).c_str());
			btn.child_of(slotModal_);
			slotRowEntities_.push_back(btn);
		}
		else if (occupied) {
			const auto loadBtn = ui::CreateButton(
									 world,
									 {.x = mx + 256.0f, .y = rowY, .w = 200.0f, .h = 44.0f},
									 "Load",
									 [this, i](const flecs::entity& e) {
										 const flecs::world wr = e.world();
										 BindSession(wr, i, /*load=*/true);
										 CloseSlotModal();
										 scene::ActivateScene<FarmWorldSceneTag>(wr);
									 }
			);
			loadBtn.set_name(("SlotLoad" + std::to_string(i + 1)).c_str());
			loadBtn.child_of(slotModal_);
			slotRowEntities_.push_back(loadBtn);

			const auto deleteBtn = ui::CreateButton(
									   world,
									   {.x = mx + 468.0f, .y = rowY, .w = 148.0f, .h = 44.0f},
									   "Delete",
									   [this, i](const flecs::entity& e) {
										   const flecs::world wr = e.world();
										   DeleteFarmSlot(wr, i);
										   RebuildSlotRows(wr);
										   RefreshContinue(wr);
									   }
			);
			deleteBtn.set_name(("SlotDelete" + std::to_string(i + 1)).c_str());
			deleteBtn.child_of(slotModal_);
			slotRowEntities_.push_back(deleteBtn);
		}
	}
}

} // namespace game::farm
