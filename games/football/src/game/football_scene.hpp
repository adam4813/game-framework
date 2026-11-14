#pragma once

#include <functional>
#include <string>

#include <flecs.h>

#include "engine/engine.hpp"
#include "football/football.hpp"

namespace game {

// Football prototype scene. Each down you draw a hand of cards from your deck (a persistent
// whole-game deck) plus two always-available basic plays: most cards are plays, some are
// utility/buff/debuff modifiers you play onto the hand to reshape the plays' odds. Select a play and
// Hike it — a quick animation resolves it and marches a token up a multi-segment distance bar.
//
// The game phase is a flecs-idiomatic state machine: a "flow" entity carries exactly one phase tag
// (phase::PlaySelect / Resolve / SegmentEnd / Shop / Results). Observers fire the phase's entry
// logic on tag add, a system keyed on phase::Resolve drives the pick animation, and a system keyed
// on RebuildUITag rebuilds the current phase's UI (requested on entry and on intra-phase changes).
class FootballScene : public engine::scene::SceneBase {
public:
	FootballScene() = default;
	~FootballScene() override = default;

	void InitializePipeline(const flecs::world& world) override;
	void Load(flecs::world& world) override;
	void Unload(flecs::world& world) override;

private:
	// Registers the phase observers + systems (children of systemsRoot_, torn down on Unload).
	void RegisterFlowSystems(const flecs::world& world);

	// Phase-entry logic (run by the OnAdd observers).
	void OnEnterPlaySelect(const flecs::world& world);
	void OnEnterResolve(const flecs::world& world);
	void OnEnterSegmentEnd(const flecs::world& world);
	void OnEnterShop(const flecs::world& world);

	void RebuildCurrentPhase(const flecs::world& world);
	void AnimateResolve(const flecs::world& world, float t) const;

	// Textured button factory (applies the shared N64 button texture).
	flecs::entity
	Btn(const flecs::world& world,
		engine::core::Rect rect,
		std::string label,
		std::function<void(flecs::entity)> on_click) const;

	void BuildHud(const flecs::world& world, flecs::entity parent) const;
	void BuildDistanceBar(const flecs::world& world, flecs::entity parent) const;
	void BuildHand(const flecs::world& world, flecs::entity parent);
	void BuildDeckOverlay(const flecs::world& world, flecs::entity parent);
	void BuildResult(const flecs::world& world, flecs::entity parent);
	void BuildSegmentEnd(const flecs::world& world, flecs::entity parent);
	void BuildShop(const flecs::world& world, flecs::entity parent);
	void BuildResults(const flecs::world& world, flecs::entity parent);

	flecs::entity uiRoot_;
	flecs::entity contentRoot_;
	flecs::entity flowEntity_;  // carries the current phase tag (+ RebuildUITag)
	flecs::entity systemsRoot_; // parent of the phase observers/systems

	// N64-era texture handles (loaded in Load()).
	int texField_{-1};
	int texPanel_{-1};
	int texButton_{-1};
	int texBoard_{-1};
	int texMenu_{-1};

	// Retained panels animated per-frame during the resolve phase (ball, receiver, defender).
	flecs::entity animBall_;
	flecs::entity animReceiver_;
	flecs::entity animDefender_;

	float phaseTime_{0.0F};      // seconds in the current phase (drives the resolve animation)
	bool resultRevealed_{false}; // resolve phase: has the animation finished and the result shown?
	bool deckOpen_{false};       // is the deck-inspection overlay open?
	bool newHighScore_{false};   // results phase: did this run set a new high score?
};

} // namespace game
