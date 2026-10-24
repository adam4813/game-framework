#include "controls.hpp"

#include <algorithm>
#include <array>

#include "engine/engine.hpp"

using namespace engine;

namespace crabnado {

namespace {

// A key that pushes the move action along the ground plane.
struct MoveBinding {
	int key;
	glm::vec2 direction;
};

constexpr std::array kMoveBindings{
	MoveBinding{.key = input::KeyCode::W, .direction = {0.0F, -1.0F}},
	MoveBinding{.key = input::KeyCode::Up, .direction = {0.0F, -1.0F}},
	MoveBinding{.key = input::KeyCode::S, .direction = {0.0F, 1.0F}},
	MoveBinding{.key = input::KeyCode::Down, .direction = {0.0F, 1.0F}},
	MoveBinding{.key = input::KeyCode::A, .direction = {-1.0F, 0.0F}},
	MoveBinding{.key = input::KeyCode::Left, .direction = {-1.0F, 0.0F}},
	MoveBinding{.key = input::KeyCode::D, .direction = {1.0F, 0.0F}},
	MoveBinding{.key = input::KeyCode::Right, .direction = {1.0F, 0.0F}},
};

// The key held to dig in.
constexpr int kDigInKey = input::KeyCode::Space;

// The key pressed to pause or resume.
constexpr int kPauseKey = input::KeyCode::Escape;

// Keys that pick molt upgrade 1, 2 and 3.
constexpr std::array kPickKeys{input::KeyCode::Key1, input::KeyCode::Key2, input::KeyCode::Key3};

} // namespace

void RegisterControls(const flecs::world& world) {
	world.component<PlayerIntent>().member<glm::vec2>("move").member<int>("pick").member<bool>("dig_in").member<bool>("pause");
	world.set<PlayerIntent>({});

	world.system<const input::InputState, PlayerIntent>("ReadControls")
		.term_at(0)
		.singleton()
		.term_at(1)
		.singleton()
		.kind(flecs::PreUpdate)
		.each([](const input::InputState& input, PlayerIntent& intent) {
			glm::vec2 move{0.0F};
			for (const auto& [key, direction] : kMoveBindings) {
				if (input.IsKeyDown(key)) {
					move += direction;
				}
			}
			// W and Up together still move at full speed, not double.
			intent.move = glm::clamp(move, glm::vec2{-1.0F}, glm::vec2{1.0F});

			intent.dig_in = input.IsKeyDown(kDigInKey);
			intent.pause = input.WasKeyPressed(kPauseKey);

			intent.pick = 0;
			for (size_t i = 0; i < kPickKeys.size(); ++i) {
				if (input.WasKeyPressed(kPickKeys[i])) {
					intent.pick = static_cast<int>(i) + 1;
				}
			}
		});
}

} // namespace crabnado
