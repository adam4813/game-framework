#pragma once

#include <flecs.h>

#include "operator_components.hpp"
#include "station_catalogue.hpp"
#include "station_components.hpp"

namespace signal_room {

// Raise a Refusal on `op`: its press at `target` could do nothing. Changes nothing else.
void Refuse(const flecs::world& world, flecs::entity op, flecs::entity target, RefusalReason reason);

// The Transform of `station`'s kind that would take the Item `carried` into a free input slot right now (the
// Station is Idle or Loading and the Item fits), or nullptr. PressStation's carrying rule and the where-to-go
// chevrons both ask this, so they can't disagree.
const StationTransform* TransformTaking(const flecs::world& world, flecs::entity station, flecs::entity carried);

// Resolve `op`'s `interact` press at the in-service `station` it Targets, by the Interact rules:
// - carrying: place the Item if the Job is Idle or Loading and some Transform of the kind takes it into
//   a free input slot (an Instant Transform then completes at once), else refuse;
// - empty-handed: pick up a waiting output if the Job is Done; else count a Tap press, or start a Hold
//   (a Hold verb, or a crank on a Running WalkAway); else refuse.
// `holders` finds the Operators currently Holding, so a second Operator's Hold refuses.
void PressStation(
	const flecs::world& world,
	flecs::entity op,
	flecs::entity station,
	const flecs::query<const Holding>& holders
);

// Advance a Running Job by `dt` seconds: a WalkAway runs by itself (faster while `held`, when its
// Transform has a crank), a Hold verb only while `held`. Completes the Job at progress 1.
void AdvanceJob(const flecs::world& world, flecs::entity station, float dt, bool held);

// Hand a finished Job's first output to the empty-handed Operator Targeting `station`, when its Transform
// is attended (not WalkAway). Returns whether an Operator took it.
bool AutoGrabOutput(const flecs::world& world, flecs::entity station, const flecs::query<const Operator>& operators);

// Give every in-service (not Tarped) Station under `parent` an Idle Job.
void AddStationJobs(const flecs::world& world, flecs::entity parent);

} // namespace signal_room
