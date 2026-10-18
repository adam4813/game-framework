#pragma once

#include <optional>
#include <span>
#include <string_view>

namespace engine::input {

// A KeyCode constant and its name, spelled as the constant is (`W`, `Key0`, `Up`, `LeftShift`, `F1`).
struct KeyName {
	std::string_view name;
	int code;
};

// Every named KeyCode constant (all but Unknown). The one table that data files, scripts and the
// Remote API name keys by.
[[nodiscard]] std::span<const KeyName> KeyNames();

// The KeyCode named `name` (case-sensitive), or nullopt.
[[nodiscard]] std::optional<int> KeyCodeFromName(std::string_view name);

} // namespace engine::input
