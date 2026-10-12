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
        # exercise real engine code. Farm tests use add_farm_test() in games/farm/tests/.
        function(add_engine_test test_name)
            add_executable(${test_name} ${ARGN})
            target_link_libraries(${test_name} PRIVATE GTest::gtest GTest::gtest_main engine_lib)
            gtest_discover_tests(${test_name})
        endfunction()
    else ()
        message(STATUS "Testing disabled")
    endif ()
else ()
    message(STATUS "Not main project - testing configuration skipped")
endif ()
