#pragma once

#include <vector>

#include <flecs.h>

#include "engine/engine.hpp"

namespace game::farm {

/// Identity tag for the farm title scene.
struct FarmTitleSceneTag {};

/// The farm's first scene: a simple menu offering Continue (resume the last-played slot), New Game
/// (pick a slot, then preview the world before playing), and Load (pick a saved slot to load or
/// clear). New Game and Load share one slot-picker modal, rebuilt per mode. See games/farm/docs/adr/0001.
class FarmTitleScene : public engine::scene::SceneBase {
public:
	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;

private:
	enum class SlotMode { New, Load };

	flecs::entity uiRoot_{};
	flecs::entity continueButton_{};
	flecs::entity slotModal_{};
	flecs::entity slotModalTitle_{};
	std::vector<flecs::entity> slotRowEntities_{};
	SlotMode slotMode_{SlotMode::New};

	void OpenSlotModal(const flecs::world& world, SlotMode mode);
	void CloseSlotModal();
	void RebuildSlotRows(const flecs::world& world);
	void RefreshContinue(const flecs::world& world);
};

} // namespace game::farm
