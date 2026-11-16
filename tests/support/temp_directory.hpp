#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace test_support {

// A scratch directory `<temp>/<name>`, removed on construction (in case a crashed run left one) and when the test ends.
// It is not created until something is written into it, so a test can check that nothing was.
class TempDirectory {
public:
	explicit TempDirectory(const std::string& name) : root_(std::filesystem::temp_directory_path() / name) {
		std::filesystem::remove_all(root_);
	}
	~TempDirectory() { std::filesystem::remove_all(root_); }

	TempDirectory(const TempDirectory&) = delete;
	TempDirectory& operator=(const TempDirectory&) = delete;
	TempDirectory(TempDirectory&&) = delete;
	TempDirectory& operator=(TempDirectory&&) = delete;

	// Write `text` to `relative` under the directory, creating any folders on the way.
	void Write(const std::string& relative, const std::string& text) const {
		const auto path = root_ / relative;
		std::filesystem::create_directories(path.parent_path());
		std::ofstream(path, std::ios::binary) << text;
	}

	[[nodiscard]] const std::filesystem::path& Path() const { return root_; }
	[[nodiscard]] std::string Path(const std::string& relative) const { return (root_ / relative).string(); }

private:
	std::filesystem::path root_;
};

} // namespace test_support
