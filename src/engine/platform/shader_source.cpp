#include "shader_source.hpp"

#include <fstream>
#include <optional>
#include <set>

namespace engine::platform {

namespace {

// The quoted file name of an `#include "file"` line, or nothing for any other line.
std::optional<std::string> IncludedFile(const std::string& line) {
	constexpr std::string_view kDirective = "#include";
	const std::size_t start = line.find_first_not_of(" \t");
	if (start == std::string::npos || !std::string_view{line}.substr(start).starts_with(kDirective))
		return std::nullopt;
	const std::size_t open = line.find('"', start + kDirective.size());
	if (open == std::string::npos) return std::nullopt;
	const std::size_t close = line.find('"', open + 1);
	if (close == std::string::npos) return std::nullopt;
	return line.substr(open + 1, close - open - 1);
}

// Appends `path`, with its includes expanded, to `out`. Returns the error, or nothing on success.
std::optional<std::string>
AppendFile(const std::filesystem::path& path, std::set<std::filesystem::path>& seen, std::string& out) {
	if (!seen.insert(path.lexically_normal()).second) return std::nullopt;
	std::ifstream file{path};
	if (!file) return "cannot read shader file '" + path.generic_string() + "'";
	for (std::string line; std::getline(file, line);) {
		if (line.ends_with('\r')) line.pop_back();
		if (const auto included = IncludedFile(line)) {
			if (auto error = AppendFile(path.parent_path() / *included, seen, out)) return error;
			continue;
		}
		out += line;
		out += '\n';
	}
	return std::nullopt;
}

} // namespace

std::expected<std::string, std::string>
LoadShaderSource(const std::filesystem::path& path, const std::string_view header) {
	std::set<std::filesystem::path> seen;
	std::string source;
	if (auto error = AppendFile(path, seen, source)) return std::unexpected(*std::move(error));
	if (source.starts_with("#version")) return source;
	return std::string{header} + source;
}

} // namespace engine::platform
