#pragma once

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/physics/physics_components.hpp"
#include "engine/physics/physics_types.hpp"
#include "engine/spatial/spatial.hpp"

namespace engine::physics {

class JoltBackend;

// Replay: re-simulating a predicted character after a correction. `RestoreCharacter` puts it back to a given state,
// then `StepCharacterTick` steps it once per Tick to replay.
//
// Where to call them: outside `progress()`, or from an immediate system (`.immediate()`) in the Tick before
// `PhysicsStep` (e.g. at `OnLoad`), with deferral suspended around the replay (`defer_suspend()` / `defer_resume()`)
// so a `set<CharacterMove>` between two steps reaches the next one instead of waiting in the command queue (an
// immediate system's commands are still deferred). Both use the physics step's temp allocator, which is free between
// backend calls, so they must not run while another thread is in the backend: never from a `multi_threaded` system.

/// Puts a character back to `position` (the entity's, at its feet) and `velocity`, as a Server correction gives them:
/// moves it there, re-detects its ground contacts, and writes the position to its `Transform` (never the rotation) and
/// the ground state and velocity to its `CharacterState` (when it has both, as `StepCharacterTick` does), so the
/// components match the restored state at once and the next Tick's `CharacterTeleport` doesn't move it back. Does
/// nothing for an entity without a character.
void RestoreCharacter(flecs::entity character, const glm::vec3& position, const glm::vec3& velocity);

/// Steps one character for one Tick, with the fixed dt, exactly as the fixed step's `PhysicsStep` steps it (its
/// `CharacterMove`, its `Gravity` or the world's, then `ExtendedUpdate`), and writes the result to its `Transform` and
/// `CharacterState` as `CharacterSyncFromBackend` does. It steps nothing else: no other character, no rigid body, and
/// the Tick counter doesn't move. Does nothing for an entity without a character.
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
