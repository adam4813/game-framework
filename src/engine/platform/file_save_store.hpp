#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace engine::platform {

// True for a slot name that can't escape the save directory: not empty, no path separators, no "..".
[[nodiscard]] bool IsValidSaveName(std::string_view name);

// The directory the desktop Platform keeps its saves in, `<exe dir>/saves`. Defined by the raylib backend, which owns
// the call that finds the executable's directory.
[[nodiscard]] std::filesystem::path DefaultSaveDirectory();

// The Platform save API (platform.hpp, "Persistence") backed by files: each slot is `<directory>/<name>.json`. Both
// the desktop and the headless Platform use it. The directory is created on the first write, so a store that is never
// written leaves nothing on disk. An invalid slot name (IsValidSaveName) writes nothing and reads as missing.
//
// A write is atomic: the data goes to `<name>.json.tmp`, which is then renamed over the slot, so a crash mid-write
// leaves the previous save intact. A leftover `.tmp` file is not a slot: reads, HasSave and ListSaves ignore it.
class FileSaveStore {
public:
	explicit FileSaveStore(std::filesystem::path directory) : directory_(std::move(directory)) {}

	void Write(std::string_view name, std::string_view data) const;
	// The slot's contents, or "" for a missing slot.
	[[nodiscard]] std::string Read(std::string_view name) const;
	[[nodiscard]] bool Has(std::string_view name) const;
	void Delete(std::string_view name) const;
	// Slot names without the extension, sorted.
	[[nodiscard]] std::vector<std::string> List() const;

	[[nodiscard]] const std::filesystem::path& Directory() const { return directory_; }

private:
	[[nodiscard]] std::filesystem::path SlotPath(std::string_view name) const;

	std::filesystem::path directory_;
};

} // namespace engine::platform
