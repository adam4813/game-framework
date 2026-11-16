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

// The path a reference names, made absolute and normal so two spellings of one file compare equal for cycle checks.
[[nodiscard]] fs::path RefPath(const std::string_view ref, const fs::path& base_dir) {
	return (base_dir / ref.substr(kFileRefSigil.size())).lexically_normal();
}

[[nodiscard]] fs::path Identity(const fs::path& path) {
	std::error_code ec;
	auto absolute = fs::absolute(path, ec);
	if (ec) {
		return path.lexically_normal();
	}
	return absolute.lexically_normal();
}

// Replace every reference under `node` in place. `open_files` holds the files currently being resolved, outermost
// first: a reference back to one of them is a cycle.
[[nodiscard]] bool
ResolveInPlace(json& node, const fs::path& dir, std::vector<fs::path>& open_files, const std::string_view log_tag) {
	if (IsFileRef(node)) {
		const auto ref = node.get<std::string>();
		const auto identity = Identity(RefPath(ref, dir));
		if (std::ranges::find(open_files, identity) != open_files.end()) {
			spdlog::error("[{}] File reference cycle: '{}' is already being loaded", log_tag, identity.string());
			return false;
		}
		auto loaded = LoadFileRef(ref, dir, log_tag);
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
	const auto path = RefPath(ref, base_dir);
	auto doc = LoadJsonFile(path.string(), log_tag);
	if (!doc) {
		return std::nullopt;
	}
	return FileRefDocument{.doc = std::move(*doc), .dir = path.parent_path()};
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
