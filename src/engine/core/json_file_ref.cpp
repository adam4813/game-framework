#include "json_file_ref.hpp"

#include <algorithm>
#include <string>
#include <system_error>
#include <vector>

#include <spdlog/spdlog.h>

#include "json_util.hpp"

namespace engine::core {

namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

// The path a reference names, resolved against `base_dir`.
[[nodiscard]] fs::path RefPath(const std::string_view ref, const fs::path& base_dir) {
	return (base_dir / ref.substr(kFileRefSigil.size())).lexically_normal();
}

// The file `path` really is, for cycle checks: canonical, so a case alias on a case-insensitive filesystem, a symlink or
// a junction compares equal to the file it names.
[[nodiscard]] fs::path Identity(const fs::path& path) {
	std::error_code ec;
	auto canonical = fs::weakly_canonical(path, ec);
	if (ec) {
		return fs::absolute(path, ec).lexically_normal();
	}
	return canonical;
}

// Load the file at `path` (a reference already resolved by RefPath).
[[nodiscard]] std::optional<FileRefDocument> LoadRefPath(const fs::path& path, const std::string_view log_tag) {
	auto doc = LoadJsonFile(path.string(), log_tag);
	if (!doc) {
		return std::nullopt;
	}
	return FileRefDocument{.doc = std::move(*doc), .dir = path.parent_path()};
}

// Replace every reference under `node` in place. `open_files` holds the files currently being resolved, outermost
// first: a reference back to one of them is a cycle.
[[nodiscard]] bool
ResolveInPlace(json& node, const fs::path& dir, std::vector<fs::path>& open_files, const std::string_view log_tag) {
	if (IsFileRef(node)) {
		const auto path = RefPath(node.get_ref<const std::string&>(), dir);
		const auto identity = Identity(path);
		if (std::ranges::find(open_files, identity) != open_files.end()) {
			spdlog::error("[{}] File reference cycle: '{}' is already being loaded", log_tag, identity.string());
			return false;
		}
		auto loaded = LoadRefPath(path, log_tag);
		if (!loaded) {
			return false;
		}
		open_files.push_back(identity);
		const bool ok = ResolveInPlace(loaded->doc, loaded->dir, open_files, log_tag);
		open_files.pop_back();
		if (!ok) {
			return false;
		}
		node = std::move(loaded->doc);
		return true;
	}
	if (node.is_object() || node.is_array()) {
		for (auto& child : node) {
			if (!ResolveInPlace(child, dir, open_files, log_tag)) {
				return false;
			}
		}
	}
	return true;
}

} // namespace

bool IsFileRef(const json& value) {
	return value.is_string() && value.get_ref<const std::string&>().starts_with(kFileRefSigil);
}

std::optional<FileRefDocument>
LoadFileRef(const std::string_view ref, const fs::path& base_dir, const std::string_view log_tag) {
	return LoadRefPath(RefPath(ref, base_dir), log_tag);
}

std::optional<json> LoadJsonFileResolved(const std::string_view path, const std::string_view log_tag) {
	const fs::path root = fs::path{path}.lexically_normal();
	auto doc = LoadJsonFile(root.string(), log_tag);
	if (!doc) {
		return std::nullopt;
	}
	std::vector<fs::path> open_files{Identity(root)};
	if (!ResolveInPlace(*doc, root.parent_path(), open_files, log_tag)) {
		return std::nullopt;
	}
	return doc;
}

} // namespace engine::core
