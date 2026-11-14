# Farm Game Target (ADR-0006): the farm-world-alive prototype. Built from the framework root this
# target already exists; a standalone checkout finds the installed package.
project(farm VERSION 0.1.0 LANGUAGES CXX)

if (NOT TARGET game_framework::engine_lib)
    find_package(game_framework CONFIG REQUIRED)
endif ()

# farm_lib holds every farm source except the entry point, so the unit tests link exactly the same
# code the executable runs.
file(GLOB_RECURSE FARM_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER FARM_SOURCES EXCLUDE REGEX ".*/main\.cpp$")

add_library(farm_lib STATIC ${FARM_SOURCES})
target_include_directories(farm_lib PUBLIC src)
target_compile_options(farm_lib PRIVATE ${GAME_COMPILE_OPTS})
target_link_libraries(farm_lib PUBLIC game_framework::engine_lib)
setup_tracy(farm_lib)

add_executable(farm src/main.cpp)
target_include_directories(farm PRIVATE src)
target_link_libraries(farm PRIVATE farm_lib)
setup_game_target(farm)

if (BUILD_TESTING AND NOT EMSCRIPTEN)
    add_subdirectory(tests)
endif ()
