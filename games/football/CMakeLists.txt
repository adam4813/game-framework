# Football Game Target (ADR-0006): roguelike deckbuilder prototype.
project(football VERSION 0.1.0 LANGUAGES CXX)

# Built from the framework root this target already exists; a standalone checkout finds the installed package.
if (NOT TARGET game_framework::engine_lib)
    find_package(game_framework CONFIG REQUIRED)
endif ()

file(GLOB_RECURSE FOOTBALL_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(football ${FOOTBALL_SOURCES})
target_include_directories(football PRIVATE src)
setup_game_target(football)
