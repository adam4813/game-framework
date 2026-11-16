#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "engine/core/core.hpp"

using engine::core::IsFileRef;
using engine::core::kFileRefSigil;
using engine::core::LoadJsonFileResolved;
using nlohmann::json;

namespace {

// A scratch directory under the temp directory, removed when the test ends. Write() puts a file at a relative path.
class JsonTree {
public:
	explicit JsonTree(const std::string& name) : root_(std::filesystem::temp_directory_path() / name) {
		std::filesystem::remove_all(root_);
		std::filesystem::create_directories(root_);
	}
	~JsonTree() { std::filesystem::remove_all(root_); }

	JsonTree(const JsonTree&) = delete;
	JsonTree& operator=(const JsonTree&) = delete;
	JsonTree(JsonTree&&) = delete;
	JsonTree& operator=(JsonTree&&) = delete;

	void Write(const std::string& relative, const std::string& text) const {
		const auto path = root_ / relative;
		std::filesystem::create_directories(path.parent_path());
		std::ofstream(path) << text;
	}

	[[nodiscard]] std::string Path(const std::string& relative) const { return (root_ / relative).string(); }

private:
	std::filesystem::path root_;
};

TEST(JsonFileRef, ANestedReferenceResolvesRelativeToItsOwnFile) {
	const JsonTree tree{"json_file_ref_nested"};
	tree.Write("save.json", R"({"player": "@file:actors/player.json"})");
	// The player file's reference is relative to actors/, not to save.json's folder.
	tree.Write("actors/player.json", R"({"hp": 7, "inventory": "@file:items/bag.json"})");
	tree.Write("actors/items/bag.json", R"(["sword", "lamp"])");

	const auto doc = LoadJsonFileResolved(tree.Path("save.json"), "Test");

	ASSERT_TRUE(doc.has_value());
	EXPECT_EQ(*doc, json::parse(R"({"player": {"hp": 7, "inventory": ["sword", "lamp"]}})"));
}

TEST(JsonFileRef, AReferenceInsideAnArrayAndOneAsAComponentValueBothResolve) {
	const JsonTree tree{"json_file_ref_array"};
	tree.Write(
		"level.json",
		R"({"entities": ["@file:npc.json", {"name": "Box", "components": {"material": "@file:red.json"}}]})"
	);
	tree.Write("npc.json", R"({"name": "Npc"})");
	tree.Write("red.json", R"({"color": [255, 0, 0, 255]})");

	const auto doc = LoadJsonFileResolved(tree.Path("level.json"), "Test");

	ASSERT_TRUE(doc.has_value());
	EXPECT_EQ(
		*doc,
		json::parse(
			R"({"entities": [{"name": "Npc"}, {"name": "Box", "components": {"material": {"color": [255, 0, 0, 255]}}}]})"
		)
	);
}

TEST(JsonFileRef, AMissingReferencedFileFailsTheLoad) {
	const JsonTree tree{"json_file_ref_missing"};
	tree.Write("save.json", R"({"player": "@file:nowhere.json"})");

	EXPECT_FALSE(LoadJsonFileResolved(tree.Path("save.json"), "Test").has_value());
}

TEST(JsonFileRef, AnUnparsableReferencedFileFailsTheLoad) {
	const JsonTree tree{"json_file_ref_unparsable"};
	tree.Write("save.json", R"({"player": "@file:broken.json"})");
	tree.Write("broken.json", R"({"hp": )");

	EXPECT_FALSE(LoadJsonFileResolved(tree.Path("save.json"), "Test").has_value());
}

TEST(JsonFileRef, AReferenceCycleFailsInsteadOfRecursingForever) {
	const JsonTree tree{"json_file_ref_cycle"};
	tree.Write("a.json", R"({"next": "@file:sub/b.json"})");
	tree.Write("sub/b.json", R"({"next": "@file:../a.json"})");

	EXPECT_FALSE(LoadJsonFileResolved(tree.Path("a.json"), "Test").has_value());
}

TEST(JsonFileRef, TheSameFileReferencedTwiceSideBySideIsNotACycle) {
	const JsonTree tree{"json_file_ref_diamond"};
	tree.Write("save.json", R"({"left": "@file:part.json", "right": "@file:part.json"})");
	tree.Write("part.json", R"(1)");

	const auto doc = LoadJsonFileResolved(tree.Path("save.json"), "Test");

	ASSERT_TRUE(doc.has_value());
	EXPECT_EQ(*doc, json::parse(R"({"left": 1, "right": 1})"));
}

TEST(JsonFileRef, ABareStringIsLeftAlone) {
	const JsonTree tree{"json_file_ref_bare"};
	tree.Write("save.json", R"({"name": "part.json", "list": ["part.json"], "@file:key.json": 3})");
	tree.Write("part.json", R"(1)");

	const auto doc = LoadJsonFileResolved(tree.Path("save.json"), "Test");

	ASSERT_TRUE(doc.has_value());
	EXPECT_EQ(*doc, json::parse(R"({"name": "part.json", "list": ["part.json"], "@file:key.json": 3})"));
}

TEST(JsonFileRef, AWriterBuildsAReferenceTheReaderRecognizes) {
	const json ref = std::string{kFileRefSigil} + "chunks/0.json";

	EXPECT_TRUE(IsFileRef(ref));
	EXPECT_FALSE(IsFileRef(json("chunks/0.json")));
	EXPECT_FALSE(IsFileRef(json(42)));
}

} // namespace
