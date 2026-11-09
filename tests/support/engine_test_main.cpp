// Shared main for every add_engine_test executable, in place of gtest_main.
#include <gtest/gtest.h>

#include "crash_visibility.hpp"

int main(int argc, char** argv) {
	test_support::InstallCrashVisibility();
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
