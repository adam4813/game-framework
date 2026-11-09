# Testing configuration with GoogleTest

# Only set up testing if this is the main project
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    if (BUILD_TESTING)
        enable_testing()
        message(STATUS "Testing enabled - GoogleTest will be configured")

        # Find GoogleTest in the testing context
        find_package(GTest CONFIG REQUIRED)
        include(GoogleTest)
        message(STATUS "  GoogleTest: ${GTest_VERSION} (found for testing)")

        # Helper function to add engine unit tests - MUST be defined before add_subdirectory.
        # Links engine_lib (pure engine infrastructure, no game semantics) so tests can
        # exercise real engine code. Each Game Target defines its own helper (add_<game>_test in games/<game>/).
        # Shared main for the unit tests: a crash prints its reason and never waits on a dialog.
        add_library(engine_test_main STATIC "${CMAKE_SOURCE_DIR}/tests/support/engine_test_main.cpp")
        target_include_directories(engine_test_main PUBLIC "${CMAKE_SOURCE_DIR}/tests/support")
        target_link_libraries(engine_test_main PUBLIC GTest::gtest spdlog::spdlog)

        function(add_engine_test test_name)
            add_executable(${test_name} ${ARGN})
            target_link_libraries(${test_name} PRIVATE GTest::gtest engine_test_main engine_lib)
            gtest_discover_tests(${test_name})
        endfunction()
    else ()
        message(STATUS "Testing disabled")
    endif ()
else ()
    message(STATUS "Not main project - testing configuration skipped")
endif ()
