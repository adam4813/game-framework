#include <memory>
#include <sstream>
#include <string>

#include <flecs.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include "engine/ecs/world_logger.hpp"
#include "engine/engine_context.hpp"

using engine::ecs::SetWorldLogger;
using engine::ecs::WorldLog;

namespace {

// Makes a logger named "default" writing message-only lines ("[name] text") into a string the default for one test,
// then restores the previous default logger.
class ScopedDefaultLogger {
public:
	ScopedDefaultLogger() : previous_(spdlog::default_logger()) {
		const auto sink = std::make_shared<spdlog::sinks::ostream_sink_st>(out_);
		const auto logger = std::make_shared<spdlog::logger>("default", sink);
		// "\n" in place of the platform end of line, so the expected output is the same on Windows.
		logger->set_formatter(
			std::make_unique<spdlog::pattern_formatter>("[%n] %v", spdlog::pattern_time_type::local, "\n")
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

TEST(WorldLogger, AWorldsLoggerLabelsItsLinesWithItsName) {
	const ScopedDefaultLogger log;
	flecs::world world;
	SetWorldLogger(world, "server");

	WorldLog(world).info("tick");

	EXPECT_EQ(log.Output(), "[server] tick\n");
}

TEST(WorldLogger, TwoWorldsWriteInterleavedToTheDefaultSinks) {
	const ScopedDefaultLogger log;
	flecs::world client;
	flecs::world server;
	SetWorldLogger(client, "client");
	SetWorldLogger(server, "server");

	WorldLog(client).info("one");
	WorldLog(server).info("two");
	spdlog::info("three");
	WorldLog(client).info("four");

	EXPECT_EQ(log.Output(), "[client] one\n[server] two\n[default] three\n[client] four\n");
}

TEST(WorldLogger, AWorldWithoutALoggerFallsBackToTheDefaultLogger) {
	const ScopedDefaultLogger log;
	const flecs::world world;

	WorldLog(world).info("hello");

	EXPECT_EQ(log.Output(), "[default] hello\n");
}

TEST(WorldLogger, AWorldsLoggerLogsAtTheDefaultLoggersLevel) {
	const ScopedDefaultLogger log;
	spdlog::default_logger()->set_level(spdlog::level::warn);
	flecs::world world;
	SetWorldLogger(world, "server");

	WorldLog(world).info("hidden");
	WorldLog(world).warn("shown");

	EXPECT_EQ(log.Output(), "[server] shown\n");
}

// A world given no logger still looks for one, so EngineContext must have registered WorldLogger up front: a lookup
// that registers it lazily asserts in a multi-threaded system.
TEST(WorldLogger, ASystemOnAMultiThreadedEngineWorldWithoutALoggerFallsBackToTheDefaultLogger) {
	engine::EngineContext ctx;
	auto& world = *ctx.GetWorld();
	world.set_threads(2);
	world.system("LogOnce").kind(flecs::OnUpdate).multi_threaded().run([](flecs::iter& it) {
		while (it.next()) {}
		WorldLog(it.world()).info("from a system");
	});
	const ScopedDefaultLogger log;

	world.progress();

	// The engine's own systems log too (no camera, say), so look for the line rather than match the whole output.
	EXPECT_THAT(log.Output(), testing::HasSubstr("[default] from a system\n"));
}

} // namespace
