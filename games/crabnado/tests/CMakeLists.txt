# Crabnado unit tests. Each links crabnado_lib and runs headless on the engine's HeadlessPlatform, with
# the repo assets as its asset directory. CI's Build Games workflow builds `crabnado_tests` and runs
# `ctest -L crabnado`, so every test registered here joins both.
add_custom_target(crabnado_tests)

function(add_crabnado_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE GTest::gtest engine_test_main crabnado_lib)
    target_compile_definitions(${test_name} PRIVATE CRABNADO_TEST_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
    gtest_discover_tests(${test_name} PROPERTIES LABELS crabnado)
    add_dependencies(crabnado_tests ${test_name})
endfunction()

# Title ⇄ arena scene switching through the scenes' buttons
add_crabnado_test(crabnado_scene_flow_test
    scene_flow_test.cpp
)
