#pragma once

#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "world_data.hpp"

namespace game::farm {

// Scene tag for the throwaway world-generation preview (ticket 01b). Registered from GameModule in
// debug/dev builds only.
struct WorldGenPreviewSceneTag {};

// A developer scene that makes world generation immediately inspectable: a scrollable, zoomable
// biome map on the left and a GenerationTimeline step list on the right. Selecting a timeline step
// re-renders the map to the cumulative world state at that step. A seed field regenerates in place.
//
// Rendering is deliberately immediate-mode (Platform 2D draws in DrawUI) with solid per-biome
// colours rather than the 3D tileset — the fastest, clearest surface for tuning generation, and
// superseded by the in-game map (ticket 05).
class WorldGenPreviewScene : public engine::scene::SceneBase {
public:
	std::function<void()> onBackPressed;
	std::function<void()> onPlayPressed;

	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;
	void HandleInput(const flecs::world& world, const engine::input::InputState& input) override;
	void DrawUI(const flecs::world& world, const engine::input::InputState& input) override;

private:
	WorldData world_{};
	WorldGenParams params_{}; // live-tunable generation knobs driven by the parameters panel
	int seed_{1337};
	int currentStep_{0};
	int waterStepIndex_{0};
	int riverStepIndex_{0};

	float zoom_{4.0f};              // pixels per world cell
	glm::vec2 panCentreCell_{0, 0}; // world cell shown at the map panel centre
	engine::core::Rect mapRect_{};

	flecs::entity uiRoot_{};
	flecs::entity timelineScroll_{};
	flecs::entity timelineList_{};
	flecs::entity stepLabel_{};
	flecs::entity exportStatus_{};
	flecs::entity paramsScroll_{};
	flecs::entity paramsList_{};
	std::vector<flecs::entity> stepButtons_{};

	void Regenerate(flecs::world& world);
	void BuildUI(flecs::world& world);
	void BuildParamsPanel(const flecs::world& world, const engine::core::Rect& panelRect);
	void AddParamHeader(const flecs::world& world, flecs::entity list, const std::string& text);
	void AddFloatKnob(
		const flecs::world& world,
		flecs::entity list,
		std::string name,
		float value,
		float min,
		float max,
		float step,
		std::function<void(float)> apply
	);
	void AddIntKnob(
		const flecs::world& world,
		flecs::entity list,
		std::string name,
		int value,
		int min,
		int max,
		std::function<void(int)> apply
	);
	void RebuildTimelineList(const flecs::world& world);
	void SetStep(int step);
	void FitView();
	void ExportReport(const flecs::world& world);
	void ExportImage(const flecs::world& world);

	[[nodiscard]] glm::vec2 CellToScreen(glm::vec2 cell) const;
	void DrawMap(engine::platform::Platform& platform) const;
};

} // namespace game::farm
