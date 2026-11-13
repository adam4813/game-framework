#pragma once

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/physics/physics_components.hpp"
#include "engine/physics/physics_types.hpp"
#include "engine/spatial/spatial.hpp"

namespace engine::physics {

class JoltBackend;

/// Steps one character for one Tick, with the fixed dt, exactly as the fixed step's `PhysicsStep` steps it (its
/// `CharacterMove`, its `Gravity` or the world's, then `ExtendedUpdate`), and writes the result to its `Transform` and
/// `CharacterState` as `CharacterSyncFromBackend` does. It steps nothing else: no other character, no rigid body, and
/// the Tick counter doesn't move. For re-simulating a predicted character after a correction: restore it with
/// `JoltBackend::SetCharacterPosition` and `SetCharacterVelocity`, `RefreshCharacterContacts`, then step it once per
/// Tick to replay. Does nothing for an entity without a character. Call it outside `progress()`.
void StepCharacterTick(flecs::entity character);

// The pieces `StepCharacterTick` shares with the fixed step's systems, so the two can't drift apart.

/// What a character is asked to do this Tick: its `CharacterMove` (absent: stand still) under its own `Gravity`
/// (absent: `world_gravity`).
[[nodiscard]] CharacterStepInput
MakeCharacterStepInput(const CharacterMove* move, const Gravity* gravity, const glm::vec3& world_gravity);

/// Writes a stepped character's position to `local` (never its rotation, which the game owns) and its ground state and
/// velocity to `state`, and records the position as written so `CharacterTeleport` doesn't take it for a teleport. In
/// place, without `modified<>`: `TransformPropagation` recomputes `WorldTransform` every Frame.
void WriteCharacterBack(JoltBackend& backend, flecs::entity_t entity, spatial::Transform& local, CharacterState& state);

} // namespace engine::physics
