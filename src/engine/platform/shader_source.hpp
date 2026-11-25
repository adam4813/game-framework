#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

// Turns a shader file into the source a GLSL compiler takes, so one file serves every target.
namespace engine::platform {

// Reads the shader at `path` and prepends `header` (the target's #version and precision lines). Each `#include "file"`
// line is replaced by that file, found relative to the including file; a file already included is skipped, so shared
// code needs no guards. A shader that starts with its own #version line keeps it and gets no header. The error names
// the file that couldn't be read.
std::expected<std::string, std::string> LoadShaderSource(const std::filesystem::path& path, std::string_view header);

} // namespace engine::platform
