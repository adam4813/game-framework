# Crabnado Game Target (ADR-0006). Everything except main.cpp goes into crabnado_lib so the game
# executable and its tests link the same code.
project(crabnado VERSION 0.1.0 LANGUAGES CXX)

# Built from the framework root this target already exists; a standalone checkout finds the installed package.
if (NOT TARGET game_framework::engine_lib)
    find_package(game_framework CONFIG REQUIRED)
endif ()

file(GLOB_RECURSE CRABNADO_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(REMOVE_ITEM CRABNADO_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(crabnado_lib STATIC ${CRABNADO_SOURCES})
target_include_directories(crabnado_lib PUBLIC src)
target_compile_options(crabnado_lib PRIVATE ${GAME_COMPILE_OPTS})
target_link_libraries(crabnado_lib PUBLIC game_framework::engine_lib)

add_executable(crabnado src/main.cpp)
target_link_libraries(crabnado PRIVATE crabnado_lib)
setup_game_target(crabnado)

if (BUILD_TESTING AND NOT EMSCRIPTEN)
    add_subdirectory(tests)
endif ()
