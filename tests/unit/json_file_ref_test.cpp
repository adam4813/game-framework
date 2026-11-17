#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "engine/core/core.hpp"
#include "temp_directory.hpp"

using engine::core::IsFileRef;
using engine::core::kFileRefSigil;
using engine::core::LoadJsonFileResolved;
using nlohmann::json;
using test_support::TempDirectory;

namespace {

TEST(JsonFileRef, ANestedReferenceResolvesRelativeToItsOwnFile) {
	const TempDirectory tree{"json_file_ref_nested"};
	tree.Write("save.json", R"({"player": "@file:actors/player.json"})");
	// The player file's reference is relative to actors/, not to save.json's folder.
	tree.Write("actors/player.json", R"({"hp": 7, "inventory": "@file:items/bag.json"})");
	tree.Write("actors/items/bag.json", R"(["sword", "lamp"])");

	const auto doc = LoadJsonFileResolved(tree.Path("save.json"), "Test");

	ASSERT_TRUE(doc.has_value());
	EXPECT_EQ(*doc, json::parse(R"({"player": {"hp": 7, "inventory": ["sword", "lamp"]}})"));
}

TEST(JsonFileRef, AReferenceInsideAnArrayAndOneAsAComponentValueBothResolve) {
	const TempDirectory tree{"json_file_ref_array"};
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
	const TempDirectory tree{"json_file_ref_missing"};
	tree.Write("save.json", R"({"player": "@file:nowhere.json"})");

	EXPECT_FALSE(LoadJsonFileResolved(tree.Path("save.json"), "Test").has_value());
}

TEST(JsonFileRef, AnUnparsableReferencedFileFailsTheLoad) {
	const TempDirectory tree{"json_file_ref_unparsable"};
	tree.Write("save.json", R"({"player": "@file:broken.json"})");
	tree.Write("broken.json", R"({"hp": )");

	EXPECT_FALSE(LoadJsonFileResolved(tree.Path("save.json"), "Test").has_value());
}

TEST(JsonFileRef, AReferenceCycleFailsInsteadOfRecursingForever) {
	const TempDirectory tree{"json_file_ref_cycle"};
	tree.Write("a.json", R"({"next": "@file:sub/b.json"})");
	tree.Write("sub/b.json", R"({"next": "@file:../a.json"})");

	EXPECT_FALSE(LoadJsonFileResolved(tree.Path("a.json"), "Test").has_value());
}

TEST(JsonFileRef, ACycleThroughAnotherSpellingOfTheSameFileFails) {
	const TempDirectory tree{"json_file_ref_alias_cycle"};
	// On a case-insensitive filesystem "A.json" is a.json itself; elsewhere it is missing. Either way the load fails
	// rather than recursing until the stack runs out.
	tree.Write("a.json", R"({"self": "@file:A.json"})");
	tree.Write("sub/b.json", R"({"back": "@file:./../sub/../a.json"})");
	tree.Write("root.json", R"({"start": "@file:sub/b.json"})");

	EXPECT_FALSE(LoadJsonFileResolved(tree.Path("a.json"), "Test").has_value());
	EXPECT_FALSE(LoadJsonFileResolved(tree.Path("root.json"), "Test").has_value());
}

TEST(JsonFileRef, TheSameFileReferencedTwiceSideBySideIsNotACycle) {
	const TempDirectory tree{"json_file_ref_diamond"};
	tree.Write("save.json", R"({"left": "@file:part.json", "right": "@file:part.json"})");
	tree.Write("part.json", R"(1)");

	const auto doc = LoadJsonFileResolved(tree.Path("save.json"), "Test");

	ASSERT_TRUE(doc.has_value());
	EXPECT_EQ(*doc, json::parse(R"({"left": 1, "right": 1})"));
}

TEST(JsonFileRef, ABareStringIsLeftAlone) {
	const TempDirectory tree{"json_file_ref_bare"};
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
