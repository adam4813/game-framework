#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include "engine/engine.hpp"

namespace {

using engine::render::Camera;
using engine::render::Projection;

// Captures what the default logger writes for one test, then restores the previous logger.
class ScopedDefaultLogger {
public:
	ScopedDefaultLogger() : previous_(spdlog::default_logger()) {
		const auto sink = std::make_shared<spdlog::sinks::ostream_sink_st>(out_);
		const auto logger = std::make_shared<spdlog::logger>("default", sink);
		logger->set_formatter(
			std::make_unique<spdlog::pattern_formatter>("[%l] %v", spdlog::pattern_time_type::local, "\n")
		);
		spdlog::set_default_logger(logger);
	}
	~ScopedDefaultLogger() { spdlog::set_default_logger(previous_); }

	ScopedDefaultLogger(const ScopedDefaultLogger&) = delete;
	ScopedDefaultLogger& operator=(const ScopedDefaultLogger&) = delete;
	ScopedDefaultLogger(ScopedDefaultLogger&&) = delete;
	ScopedDefaultLogger& operator=(ScopedDefaultLogger&&) = delete;

	[[nodiscard]] std::string Output() const { return out_.str(); }

private:
	std::shared_ptr<spdlog::logger> previous_;
	std::ostringstream out_;
};

struct CameraWorld {
	std::unique_ptr<engine::EngineContext> ctx =
		std::make_unique<engine::EngineContext>(std::make_unique<engine::platform::HeadlessPlatform>());

	// Loads one camera entity from a `camera` component body and returns its projection.
	Projection Load(const std::string& camera_json) const {
		flecs::world& world = *ctx->GetWorld();
		engine::level::LoadLevelFromSource(
			world, R"({"entities": [{"name": "Cam", "components": {"camera": )" + camera_json + "}}]}"
		);
		const flecs::entity cam = world.lookup("Cam");
		const Projection projection = cam.get<Camera>().projection;
		cam.destruct();
		return projection;
	}
};

TEST(CameraLoader, ProjectionLoadsPerspectiveOrthographicAndDefaultsWithoutWarning) {
	const CameraWorld w;
	const ScopedDefaultLogger log;
	EXPECT_EQ(w.Load(R"({"projection": "perspective"})"), Projection::Perspective);
	EXPECT_EQ(w.Load(R"({"projection": "orthographic"})"), Projection::Orthographic);
	EXPECT_EQ(w.Load(R"({})"), Projection::Perspective);
	EXPECT_EQ(log.Output().find("[warning]"), std::string::npos) << log.Output();
}

TEST(CameraLoader, AnUnknownProjectionWarnsOnceAndFallsBackToPerspective) {
	const CameraWorld w;
	const ScopedDefaultLogger log;
	EXPECT_EQ(w.Load(R"({"projection": "orthagraphic"})"), Projection::Perspective);

	const std::string out = log.Output();
	ASSERT_NE(out.find("orthagraphic"), std::string::npos) << out;
	EXPECT_EQ(out.find("[warning]", out.find("[warning]") + 1), std::string::npos) << out;
	EXPECT_NE(out.find("'Cam'"), std::string::npos) << out;
	EXPECT_NE(out.find("'orthographic'"), std::string::npos) << out;
}

} // namespace
