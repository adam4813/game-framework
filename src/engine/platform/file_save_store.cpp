#include "file_save_store.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#include <spdlog/spdlog.h>

namespace engine::platform {

namespace {

namespace fs = std::filesystem;

constexpr std::string_view kSlotExtension = ".json";
constexpr std::string_view kTempSuffix = ".tmp";

} // namespace

bool IsValidSaveName(const std::string_view name) {
	return !name.empty()
		   && name.find('/') == std::string_view::npos
		   && name.find('\\') == std::string_view::npos
		   && name.find("..") == std::string_view::npos;
}

fs::path FileSaveStore::SlotPath(const std::string_view name) const {
	return directory_ / (std::string{name} + std::string{kSlotExtension});
}

void FileSaveStore::Write(const std::string_view name, const std::string_view data) const {
	if (!IsValidSaveName(name)) {
		return;
	}
	std::error_code ec;
	fs::create_directories(directory_, ec);
	const fs::path slot = SlotPath(name);
	fs::path temp = slot;
	temp += kTempSuffix;
	{
		std::ofstream out(temp, std::ios::binary | std::ios::trunc);
		if (!out) {
			spdlog::error("[Save] Cannot write '{}'", temp.string());
			return;
		}
		out.write(data.data(), static_cast<std::streamsize>(data.size()));
		if (!out.flush()) {
			spdlog::error("[Save] Failed writing '{}'", temp.string());
			return;
		}
	}
	// std::filesystem::rename replaces an existing target (MoveFileEx with MOVEFILE_REPLACE_EXISTING on Windows).
	fs::rename(temp, slot, ec);
	if (ec) {
		spdlog::error("[Save] Cannot replace '{}': {}", slot.string(), ec.message());
		fs::remove(temp, ec);
	}
}

std::string FileSaveStore::Read(const std::string_view name) const {
	if (!IsValidSaveName(name)) {
		return {};
	}
	const std::ifstream in(SlotPath(name), std::ios::binary);
	if (!in) {
		return {};
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

bool FileSaveStore::Has(const std::string_view name) const {
	if (!IsValidSaveName(name)) {
		return false;
	}
	std::error_code ec;
	return fs::is_regular_file(SlotPath(name), ec);
}

void FileSaveStore::Delete(const std::string_view name) const {
	if (!IsValidSaveName(name)) {
		return;
	}
	std::error_code ec;
	fs::remove(SlotPath(name), ec);
}

std::vector<std::string> FileSaveStore::List() const {
	std::vector<std::string> names;
	std::error_code ec;
	for (fs::directory_iterator it{directory_, ec}, end; !ec && it != end; it.increment(ec)) {
		if (it->is_regular_file(ec) && it->path().extension() == kSlotExtension) {
			names.push_back(it->path().stem().string());
		}
	}
	std::ranges::sort(names);
	return names;
}

} // namespace engine::platform
