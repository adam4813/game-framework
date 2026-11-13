#include "character_step.hpp"

#include "engine/fixed_step/fixed_step.hpp"
#include "jolt_backend.hpp"

namespace engine::physics {

CharacterStepInput
MakeCharacterStepInput(const CharacterMove* move, const Gravity* gravity, const glm::vec3& world_gravity) {
	return {
		.move_velocity = move ? move->velocity : glm::vec3{0.0F},
		.gravity = gravity ? gravity->acceleration : world_gravity
	};
}

void WriteCharacterBack(
	JoltBackend& backend,
	const flecs::entity_t entity,
	spatial::Transform& local,
	CharacterState& state
) {
	if (!backend.HasCharacter(entity)) return;
	const CharacterSyncResult result = backend.GetCharacterSyncResult(entity);
	local.position = result.position;
	backend.SetCharacterWrittenPosition(entity, result.position);
	state = {
		.grounded = result.grounded,
		.ground_entity = result.ground_entity,
		.ground_normal = result.ground_normal,
		.velocity = result.velocity
	};
}

void StepCharacterTick(const flecs::entity character) {
	const flecs::world world = character.world();
	auto& backend = world.get_mut<JoltBackend>();
	if (!backend.HasCharacter(character.id())) return;

	// The dt RunTick gives PhysicsStep. No fallback: a world without the fixed step must fail here as it does there.
	const float dt = world.get<fixed_step::FixedStepConfig>().TickDeltaTime();
	const glm::vec3 world_gravity = world.get<PhysicsWorldConfig>().gravity;
	backend.StepCharacter(
		character.id(),
		dt,
		MakeCharacterStepInput(character.try_get<CharacterMove>(), character.try_get<Gravity>(), world_gravity)
	);

	auto* local = character.try_get_mut<spatial::Transform>();
	auto* state = character.try_get_mut<CharacterState>();
	if (local && state) {
		WriteCharacterBack(backend, character.id(), *local, *state);
	}
}

} // namespace engine::physics
