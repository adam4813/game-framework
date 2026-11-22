# Crabnado unit tests. Each links crabnado_lib and runs headless on the engine's HeadlessPlatform, with
# the game's own assets as its asset directory. CI's Build Games workflow builds `crabnado_tests` and runs
# `ctest -L crabnado`, so every test registered here joins both.
add_custom_target(crabnado_tests)

function(add_crabnado_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE GTest::gtest engine_test_main crabnado_lib)
    target_compile_definitions(${test_name} PRIVATE
        CRABNADO_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        CRABNADO_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    gtest_discover_tests(${test_name} PROPERTIES LABELS crabnado)
    add_dependencies(crabnado_tests ${test_name})
endfunction()

# Title ⇄ arena scene switching through the scenes' buttons
add_crabnado_test(crabnado_scene_flow_test
    scene_flow_test.cpp
)

# The wave ride's duration formula
add_crabnado_test(crabnado_ride_duration_test
    ride_duration_test.cpp
)

# The shipped campaign.json, bosses.json and enemies.json parse with every reference resolving
add_crabnado_test(crabnado_campaign_data_test
    campaign_data_test.cpp
)

# The campaign director's gates (time, quota, both), storm-up and clocks, driven headlessly on a fixture campaign
add_crabnado_test(crabnado_campaign_director_test
    campaign_director_test.cpp
)

# Campaign scripts: events and calls dispatched headlessly on a fixture campaign, refused calls, a script that throws or
# doesn't compile, the Remote API's event hook, and the shipped scripts' handlers against the event table
add_crabnado_test(crabnado_campaign_script_test
    campaign_script_test.cpp
)
