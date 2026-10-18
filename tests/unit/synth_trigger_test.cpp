#include <gtest/gtest.h>

#include "engine/audio/synth_trigger.hpp"

using namespace engine::audio;

TEST(SynthTriggerTest, a_burst_of_fires_between_callbacks_plays_once_not_as_a_burst) {
	// While web audio is suspended the callback doesn't run; the backlog must not replay on unlock.
	SynthTrigger trigger;
	EXPECT_FALSE(trigger.Consume());

	trigger.Fire();
	trigger.Fire();
	trigger.Fire();
	EXPECT_TRUE(trigger.Consume());
	EXPECT_FALSE(trigger.Consume());

	trigger.Fire();
	EXPECT_TRUE(trigger.Consume());
}
