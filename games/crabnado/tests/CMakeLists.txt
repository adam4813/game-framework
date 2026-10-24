# Crabnado unit tests. Each links crabnado_lib and runs headless on the engine's NullPlatform, with
# the repo assets as its asset directory.
function(add_crabnado_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE GTest::gtest GTest::gtest_main crabnado_lib)
    target_include_directories(${test_name} PRIVATE "${CMAKE_SOURCE_DIR}/tests/scene_equivalence")
    target_compile_definitions(${test_name} PRIVATE CRABNADO_TEST_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
    gtest_discover_tests(${test_name})
endfunction()

# Title ⇄ arena scene switching through the scenes' buttons
add_crabnado_test(crabnado_scene_flow_test
    scene_flow_test.cpp
)
