#include "operator_input.hpp"

#include "bindings.hpp"
#include "engine/engine.hpp"
#include "operator_components.hpp"

using namespace engine;

namespace signal_room {

namespace {

// The normalised direction the movement Actions ask for (zero when none or they cancel out).
glm::vec2 WishDirection(const Bindings& bindings, const input::InputState& input) {
	const auto axis = [&](const Action negative, const Action positive) {
		return (bindings.IsDown(input, positive) ? 1.0F : 0.0F) - (bindings.IsDown(input, negative) ? 1.0F : 0.0F);
	};
	const glm::vec2 wish{axis(Action::MoveLeft, Action::MoveRight), axis(Action::MoveUp, Action::MoveDown)};
	return wish == glm::vec2{0.0F} ? wish : glm::normalize(wish);
}

} // namespace

void RegisterOperatorInput(const flecs::world& world, const flecs::entity roomRoot) {
	// One keyboard table drives every Operator until spec 3 gives each Seat its own bindings.
	world.system<OperatorActions>("OperatorInput")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, OperatorActions& actions) {
			const auto& bindings = it.world().get<Bindings>();
			const auto& input = it.world().get<input::InputState>();
			actions = {
				.move = WishDirection(bindings, input),
				.dash = bindings.WasPressed(input, Action::Dash),
				.interact = bindings.WasPressed(input, Action::Interact),
				.interactHeld = bindings.IsDown(input, Action::Interact),
			};
		})
		.child_of(roomRoot);
}

} // namespace signal_room
