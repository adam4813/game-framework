#pragma once

#include <array>
#include <expected>
#include <string>
#include <vector>

#include "engine/input/input_components.hpp"

namespace signal_room {

// What a player can ask an Operator to do. Game logic reads Actions, never raw key codes, so spec 3
// can swap the keyboard table for per-Operator bindings. The set is fixed by the game's code (each
// Action is read somewhere), so it is a closed enum; the bindings file names them as strings.
enum class Action { MoveUp, MoveDown, MoveLeft, MoveRight, Dash, Interact, Count };

inline constexpr size_t kActionCount = static_cast<size_t>(Action::Count);

// The name each Action has in the bindings file, indexed by Action.
inline constexpr std::array<const char*, kActionCount> kActionNames{
	"move_up",
	"move_down",
	"move_left",
	"move_right",
	"dash",
	"interact",
};

// World singleton loaded from the bindings file: the keys bound to each Action (any of them works).
struct Bindings {
	std::array<std::vector<int>, kActionCount> keys;

	// True while any key bound to `action` is held.
	[[nodiscard]] bool IsDown(const engine::input::InputState& input, Action action) const;
	// True on the frame any key bound to `action` goes down.
	[[nodiscard]] bool WasPressed(const engine::input::InputState& input, Action action) const;
};

// Parse a bindings file: {"actions": {"move_up": ["W", "Up"], ...}} with every Action listed and keys
// named as engine KeyCode constants (`W`, `Up`, `LeftShift`, `Space`, ...). The error names the file
// and the bad Action or key.
[[nodiscard]] std::expected<Bindings, std::string> LoadBindingsFile(const std::string& path);

} // namespace signal_room
