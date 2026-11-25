#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "engine/platform/shader_source.hpp"

using engine::platform::LoadShaderSource;

namespace {

constexpr auto kHeader = "#version 330\n";

// A directory of shader files, removed with the test.
class ShaderSource : public testing::Test {
protected:
	void SetUp() override {
		dir = std::filesystem::temp_directory_path()
			  / (std::string("shader_source_") + testing::UnitTest::GetInstance()->current_test_info()->name());
		std::filesystem::create_directories(dir / "lib");
	}
	void TearDown() override { std::filesystem::remove_all(dir); }

	void Write(const std::string& name, const std::string& text) const {
		std::ofstream{dir / name, std::ios::binary} << text;
	}

	std::filesystem::path dir;
};

} // namespace

TEST_F(ShaderSource, TheHeaderGoesFirst) {
	Write("a.fs", "void main() {}\n");
	EXPECT_EQ(LoadShaderSource(dir / "a.fs", kHeader), "#version 330\nvoid main() {}\n");
}

TEST_F(ShaderSource, AShaderWithItsOwnVersionKeepsIt) {
	Write("a.fs", "#version 100\nvoid main() {}\n");
	EXPECT_EQ(LoadShaderSource(dir / "a.fs", kHeader), "#version 100\nvoid main() {}\n");
}

TEST_F(ShaderSource, AnIncludeIsReplacedByTheFileNextToIt) {
	Write("lib/shared.glsl", "#include \"more.glsl\"\nfloat shared;\n");
	Write("lib/more.glsl", "float more;\n");
	Write("a.fs", "#include \"lib/shared.glsl\"\nvoid main() {}\n");
	EXPECT_EQ(LoadShaderSource(dir / "a.fs", kHeader), "#version 330\nfloat more;\nfloat shared;\nvoid main() {}\n");
}

TEST_F(ShaderSource, AFileIsIncludedOnce) {
	Write("shared.glsl", "float shared;\n");
	Write("a.fs", "#include \"shared.glsl\"\n#include \"shared.glsl\"\n#include \"a.fs\"\nvoid main() {}\n");
	EXPECT_EQ(LoadShaderSource(dir / "a.fs", kHeader), "#version 330\nfloat shared;\nvoid main() {}\n");
}

TEST_F(ShaderSource, WindowsLineEndingsAreDropped) {
	Write("a.fs", "float x;\r\nvoid main() {}\r\n");
	EXPECT_EQ(LoadShaderSource(dir / "a.fs", kHeader), "#version 330\nfloat x;\nvoid main() {}\n");
}

TEST_F(ShaderSource, AMissingFileIsNamedInTheError) {
	Write("a.fs", "#include \"nowhere.glsl\"\n");
	const auto missing_include = LoadShaderSource(dir / "a.fs", kHeader);
	ASSERT_FALSE(missing_include);
	EXPECT_NE(missing_include.error().find("nowhere.glsl"), std::string::npos);

	const auto missing_shader = LoadShaderSource(dir / "b.fs", kHeader);
	ASSERT_FALSE(missing_shader);
	EXPECT_NE(missing_shader.error().find("b.fs"), std::string::npos);
}
