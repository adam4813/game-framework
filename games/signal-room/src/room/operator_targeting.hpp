#pragma once

#include <flecs.h>

namespace signal_room {

// Register the OperatorTargeting system as a child of the Room's root entity. Each frame it picks
// every Operator's Target from its position and facing (call after RegisterOperatorMovement, so it
// sees this frame's move) and stores it as the (Targets, entity) pair:
// - the cell TargetingTuning::rayLength ahead, if it is a Counter, or an in-service Station the
//   Operator stands on a worked side of;
// - otherwise the nearest such thing within coneRange and coneHalfAngle of facing;
// - otherwise none. Tarped Stations, props and walls are never candidates.
void RegisterOperatorTargeting(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
