#pragma once

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <flecs.h>

#include "campaign.hpp"
#include "engine/engine.hpp"

// Campaign scripts: an Act or a Stage names an AngelScript file (`assets/scripts/crabnado/<id>.as`) that handles
// discrete campaign events and can end the Stage, bring the boss, spawn enemies and show a banner or sting, without
// changing any data. A script defines only the handlers it wants; nothing runs per frame, and `After(seconds, name)`
// fires `OnTimer(name)`.
//
// Events (declaration as the script writes it; a handler's parameter names are free):
//   void OnActStart()                       the Act begins
//   void OnStageStart()                     a Stage (the Boss Stage included) begins
//   void OnStageEnd(const string &in reason)  "time", "quota", "script" (EndStage) or "boss" (the Act's boss is beaten)
//   void OnBossStart(const string &in id)   the Act's boss comes ashore
//   void OnBossPhase(const string &in id, int phase)  the boss enters its next phase (1 at the start, then one per slam)
//   void OnBossDefeated(const string &in id)
//   void OnKill(const string &in enemyType, const string &in source)  `source` is the spawner that brought it in
//   void OnTimer(const string &in name)     an After() timer ran out (only the script that set it hears it)
// Calls (global functions): EndStage(), SpawnBoss(id), Spawn(type, count, source), Banner(text), Sting(id),
// After(seconds, name), and the read-only StageClock(), Kills() (the run's total), Act() and Stage() (0-based, as in
// RunState; the Boss Stage is the Act's Stage count). There are no setters for spawn tables, rates, HP or enemy stats.
//
// Dispatch runs the Act's handler, then the Stage's. A call a handler makes is queued, and the queue applies in call
// order at the end of the campaign director's tick. A call that doesn't fit the phase (EndStage in a Boss Stage, a molt
// or a Transition; SpawnBoss outside the Act's Boss Stage or with the boss up; Spawn in a molt or between Acts; any call
// outside a handler) is logged and ignored. A handler that throws is logged once and disables that script for the run.
// Scripts load with their Act or Stage, and are rebuilt on every arena Load; the Act's script keeps its globals through the
// Act's Stages, a Stage's script starts fresh each Stage.
namespace crabnado {

// One entry of the event table.
struct CampaignEventSpec {
	std::string_view name;
	// The handler's declaration, as AngelScript's GetFunctionByDecl takes it.
	std::string_view declaration;
	// One letter per argument: 's' a string, 'i' an int (how the Remote API's fire_arg fields are read).
	std::string_view argKinds;
};

// The eight events, in dispatch-table order.
[[nodiscard]] std::span<const CampaignEventSpec> CampaignEvents();

// Which events a script's global functions define, and the On* functions that match no event (a typo or the wrong
// parameters, so the handler would never run). `declarations` is IScriptInstance::FunctionDeclarations().
struct HandlerReport {
	std::vector<std::string> defined;
	std::vector<std::string> unmatched;
};
[[nodiscard]] HandlerReport AnalyzeHandlers(std::span<const std::string> declarations);

// Name of the world entity whose `timer::Cooldown` counts down the Banner() display (a Remote API readout).
inline constexpr const char* kScriptBannerName = "::ScriptBanner"; // absolute: registered inside a module scope

// Singleton, reflected: what the scripts last showed, and a Remote API hook. `banner` is the text of the last Banner()
// call while the `timer::Cooldown` on the entity named kScriptBannerName is running. Writing `fire_event` (an event name) with `fire_arg0` / `fire_arg1` as its
// arguments fires that event on the next director tick, then clears `fire_event`.
struct CampaignScripts {
	std::string banner;
	std::string fire_event;
	std::string fire_arg0;
	std::string fire_arg1;
};

// The file a script id names, in the CampaignSource's scripts directory.
[[nodiscard]] std::string ScriptPath(const flecs::world& world, std::string_view id);

// Registers and reflects the campaign script singletons and binds the `campaign` API with the scripting module (a no-op
// binding without one). Call from the module constructor after RegisterCampaign.
void RegisterCampaignScripts(const flecs::world& world);

// Unloads every script and clears pending calls and timers; the host entity is made a child of `sceneRoot`, so the
// scene's Unload destroys it. Call from the arena scene's Load.
void ResetCampaignScripts(const flecs::world& world, flecs::entity sceneRoot);

// Unloads every script without firing anything. Call from the arena scene's Unload.
void UnloadCampaignScripts(const flecs::world& world);

// Loads the Act's or the Stage's script, replacing the one loaded before; an empty `id` just unloads it. A script that
// fails to compile is logged and skipped. The Act's script is loaded at Act start, the Stage's at Stage start.
void LoadActScript(const flecs::world& world, std::string_view id);
void LoadStageScript(const flecs::world& world, std::string_view id);

// Whether the Stage's script is loaded and hasn't died: a `script` gate with none falls back to a time gate.
[[nodiscard]] bool StageScriptAlive(const flecs::world& world);

// The Act ended with its boss beaten: fires OnStageEnd("boss"), then unloads the Act's and the Stage's scripts.
void EndActScripts(const flecs::world& world);

// Fires an event on the Act's script, then the Stage's, each only if it defines the handler. An unknown event name is
// logged. Calls the handlers make are queued until the next StepCampaignScripts.
void FireCampaignEvent(const flecs::world& world, std::string_view event, std::span<const engine::scripting::ScriptArg> args);
void FireCampaignEvent(const flecs::world& world, std::string_view event, std::initializer_list<engine::scripting::ScriptArg> args = {});

// One director tick of the scripts: runs the banner and After() timers, fires a Remote API event request, then applies
// the queued calls (see the file comment).
void StepCampaignScripts(const flecs::world& world, float dt);

} // namespace crabnado
