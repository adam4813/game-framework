# Sandbox Game Target (ADR-0006): engine demonstration scenes used for engine feature development.
project(sandbox VERSION 0.1.0 LANGUAGES CXX)

# Built from the framework root this target already exists; a standalone checkout finds the installed package.
if (NOT TARGET game_framework::engine_lib)
    find_package(game_framework CONFIG REQUIRED)
endif ()

file(GLOB_RECURSE SANDBOX_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(sandbox ${SANDBOX_SOURCES})
target_include_directories(sandbox PRIVATE src)
setup_game_target(sandbox)
