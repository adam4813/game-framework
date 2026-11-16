#include "file_save_store.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#include <spdlog/spdlog.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace engine::platform {

namespace {

namespace fs = std::filesystem;

constexpr std::string_view kSlotExtension = ".json";
constexpr std::string_view kTempSuffix = ".tmp";

// The running executable's directory (what the desktop Platform's assets sit beside), or the working directory when
// the OS won't say.
[[nodiscard]] fs::path ExecutableDirectory() {
#if defined(_WIN32)
	std::wstring buffer(MAX_PATH, L'\0');
	for (;;) {
		const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (length == 0) {
			break;
		}
		if (length < buffer.size()) {
			buffer.resize(length);
			return fs::path{buffer}.parent_path();
		}
		buffer.resize(buffer.size() * 2);
	}
#elif defined(__APPLE__)
	std::uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	std::string buffer(size, '\0');
	if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
		std::error_code ec;
		const fs::path exe{buffer.c_str()};
		const auto resolved = fs::weakly_canonical(exe, ec);
		return (ec ? exe : resolved).parent_path();
	}
#elif !defined(__EMSCRIPTEN__)
	std::error_code ec;
	const auto exe = fs::read_symlink("/proc/self/exe", ec);
	if (!ec) {
		return exe.parent_path();
	}
#endif
	std::error_code cwd_ec;
	return fs::current_path(cwd_ec);
}

// Write `data` to `path`, replacing it, and flush it to the disk before returning, so the rename that follows can't
// expose a slot whose contents the OS hadn't stored yet. Returns false (with a log line) on any failure.
[[nodiscard]] bool WriteDurably(const fs::path& path, const std::string_view data) {
	bool ok = true;
#if defined(_WIN32)
	int fd = -1;
	if (_wsopen_s(&fd, path.c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE)
		!= 0) {
		spdlog::error("[Save] Cannot write '{}'", path.string());
		return false;
	}
	constexpr std::size_t kMaxChunk = 1U << 30U;
	for (std::size_t written = 0; ok && written < data.size();) {
		const auto chunk = static_cast<unsigned>(std::min(data.size() - written, kMaxChunk));
		const int n = _write(fd, data.data() + written, chunk);
		ok = n > 0;
		written += ok ? static_cast<std::size_t>(n) : 0;
	}
	ok = ok && _commit(fd) == 0;
	ok = _close(fd) == 0 && ok;
#else
	const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		spdlog::error("[Save] Cannot write '{}'", path.string());
		return false;
	}
	for (std::size_t written = 0; ok && written < data.size();) {
		const ssize_t n = ::write(fd, data.data() + written, data.size() - written);
		ok = n > 0;
		written += ok ? static_cast<std::size_t>(n) : 0;
	}
	ok = ok && ::fsync(fd) == 0;
	ok = ::close(fd) == 0 && ok;
#endif
	if (!ok) {
		spdlog::error("[Save] Failed writing '{}'", path.string());
	}
	return ok;
}

// Make the rename itself durable on POSIX by flushing the directory entry. Windows has no directory handle to flush
// here; NTFS journals the rename's metadata.
void SyncDirectory([[maybe_unused]] const fs::path& dir) {
#if !defined(_WIN32)
	if (const int fd = ::open(dir.c_str(), O_RDONLY); fd >= 0) {
		::fsync(fd);
		::close(fd);
	}
#endif
}

} // namespace

bool IsValidSaveName(const std::string_view name) {
	return !name.empty()
		   && name.find('/') == std::string_view::npos
		   && name.find('\\') == std::string_view::npos
		   && name.find("..") == std::string_view::npos;
}

fs::path DefaultSaveDirectory() {
#if defined(__EMSCRIPTEN__)
	// The web game keeps its saves in localStorage; this is only where a headless Platform's files go, in the
	// in-memory filesystem.
	return "/saves";
#else
	return ExecutableDirectory() / "saves";
#endif
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
	if (!WriteDurably(temp, data)) {
		fs::remove(temp, ec);
		return;
	}
	// std::filesystem::rename replaces an existing target (MoveFileEx with MOVEFILE_REPLACE_EXISTING on Windows).
	fs::rename(temp, slot, ec);
	if (ec) {
		spdlog::error("[Save] Cannot replace '{}': {}", slot.string(), ec.message());
		fs::remove(temp, ec);
		return;
	}
	SyncDirectory(directory_);
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
