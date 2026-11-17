#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

namespace engine::core {

// A File Reference: a JSON string `"@file:<path>"` standing in for the whole JSON document at `<path>`. The path is
// relative to the directory of the file holding the reference, so a document split across files stays portable when
// its folder moves. Only strings that start with the sigil are references; every other string is a plain value. A
// writer emits a reference as `std::string{kFileRefSigil} + "relative/path.json"`.
inline constexpr std::string_view kFileRefSigil = "@file:";

// True when `value` is a string starting with kFileRefSigil.
[[nodiscard]] bool IsFileRef(const nlohmann::json& value);

// The document one File Reference names, and the directory its own references resolve against.
struct FileRefDocument {
	nlohmann::json doc;
	std::filesystem::path dir;
};

// Load the file `ref` names (an `"@file:"` string), resolving its path against `base_dir`. The returned document is
// as parsed: references inside it are left in place, for a caller that resolves them itself (the level loader builds
// entities as it goes). Returns std::nullopt, logged under `log_tag`, when the file is missing or isn't valid JSON.
// Precondition: `ref` starts with kFileRefSigil.
[[nodiscard]] std::optional<FileRefDocument>
LoadFileRef(std::string_view ref, const std::filesystem::path& base_dir, std::string_view log_tag);

// Load the JSON file at `path` with every File Reference in it replaced by the document it names, at any depth (an
// object member, an array element, inside a referenced file). Each reference resolves against the directory of the
// file that contains it. Object keys are never references. Returns std::nullopt, logged under `log_tag`, when any
// file in the tree is missing or isn't valid JSON, or when a reference cycles back to a file it is already inside.
// The same file referenced twice side by side is not a cycle and is loaded for each reference.
[[nodiscard]] std::optional<nlohmann::json> LoadJsonFileResolved(std::string_view path, std::string_view log_tag);

} // namespace engine::core
