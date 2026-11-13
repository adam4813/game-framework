#include <cstdlib>
#include <string>

#include <flecs.h>
#include <gtest/gtest.h>

#include "engine/ecs/flecs_remote.hpp"

using engine::ecs::InitializeRemoteAPI;
using engine::ecs::kRestPortEnv;
using engine::ecs::RestPortEnv;

namespace {

// Ports no game or test default uses, so the servers these tests start don't collide with a running game.
constexpr int kGivenPort = 27791;
constexpr int kEnvPort = 27792;

// Sets FLECS_REST_PORT for one test, then restores the value it had (or unsets it).
class ScopedRestPortEnv {
public:
	explicit ScopedRestPortEnv(const int port) : previous_(Get()) { Set(std::to_string(port).c_str()); }
	~ScopedRestPortEnv() { Set(previous_.c_str()); }

	ScopedRestPortEnv(const ScopedRestPortEnv&) = delete;
	ScopedRestPortEnv& operator=(const ScopedRestPortEnv&) = delete;
	ScopedRestPortEnv(ScopedRestPortEnv&&) = delete;
	ScopedRestPortEnv& operator=(ScopedRestPortEnv&&) = delete;

private:
	// The variable's value, or empty when unset (an empty value can't be set on Windows anyway).
	static std::string Get() {
#if defined(_WIN32)
		char* buffer = nullptr;
		size_t length = 0;
		if (_dupenv_s(&buffer, &length, kRestPortEnv) != 0 || buffer == nullptr) {
			return {};
		}
		std::string value{buffer};
		std::free(buffer);
		return value;
#else
		const char* value = std::getenv(kRestPortEnv);
		return value != nullptr ? std::string{value} : std::string{};
#endif
	}

	// An empty value unsets the variable on both platforms.
	static void Set(const char* value) {
#if defined(_WIN32)
		_putenv_s(kRestPortEnv, value);
#else
		if (value[0] == '\0') {
			unsetenv(kRestPortEnv);
		}
		else {
			setenv(kRestPortEnv, value, 1);
		}
#endif
	}

	std::string previous_;
};

int ServedPort(const flecs::world& world) { return world.get<flecs::Rest>().port; }

TEST(RestPortOverride, AWorldHonorsTheEnvironmentPortByDefault) {
	const ScopedRestPortEnv env{kEnvPort};
	flecs::world world;

	InitializeRemoteAPI(world, kGivenPort);

	EXPECT_EQ(ServedPort(world), kEnvPort);
}

TEST(RestPortOverride, AWorldThatIgnoresTheEnvironmentServesOnItsGivenPort) {
	const ScopedRestPortEnv env{kEnvPort};
	flecs::world world;

	InitializeRemoteAPI(world, kGivenPort, RestPortEnv::Ignore);

	EXPECT_EQ(ServedPort(world), kGivenPort);
}

} // namespace
