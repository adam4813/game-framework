# ── Signal Room Game Target (ADR-0006) ───────────────────────────────────────
# signal_room_lib holds the game code (everything under src/ except the entry point); the thin
# signal_room executable adds main.cpp, and the tests link the same library.

file(GLOB_RECURSE SIGNAL_ROOM_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(REMOVE_ITEM SIGNAL_ROOM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(signal_room_lib STATIC ${SIGNAL_ROOM_SOURCES})
target_include_directories(signal_room_lib PUBLIC src)
target_compile_options(signal_room_lib PRIVATE ${GAME_COMPILE_OPTS})
target_link_libraries(signal_room_lib PUBLIC engine_lib)
setup_tracy(signal_room_lib)

add_executable(signal_room src/main.cpp)
target_link_libraries(signal_room PRIVATE signal_room_lib)
# The engine needs its shaders (and the optional profiling filter) from the root assets; everything
# else comes from the game's own assets/ folder, which setup_game_target layers over them.
setup_game_target(signal_room ROOT_ASSETS shaders config)

# ── Tests ────────────────────────────────────────────────────────────────────
if (BUILD_TESTING AND NOT EMSCRIPTEN AND CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Mirrors add_engine_test: links the game library so tests exercise the code that ships.
    function(add_signal_room_test test_name)
        add_executable(${test_name} ${ARGN})
        target_link_libraries(${test_name} PRIVATE GTest::gtest engine_test_main signal_room_lib)
        gtest_discover_tests(${test_name})
    endfunction()

    add_subdirectory(tests)
endif ()
