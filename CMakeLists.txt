cmake_minimum_required(VERSION 3.30)
project(game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SCENE_EQUIVALENCE_TEST "Build the scene equivalence test application" ON)
# Enable Tracy frame profiler integration. Only meaningful on desktop (no WASM support).
# Activates the Flecs OS API perf hooks, explicit render zones, and the FrameMark.
option(ENABLE_TRACY "Enable Tracy profiler integration" OFF)
# Auto-start JSONL telemetry on launch so stats-telemetry.jsonl is written immediately.
# The in-game checkbox still works; this just flips the initial state.
option(ENABLE_PROFILE_LOGGING "Auto-start JSONL system-stats telemetry on launch" OFF)
# Dear ImGui debug UI (toggle with backtick). OFF compiles ImGui out entirely: Platform::GetDebugUi()
# then returns a no-op DebugUi, so game panel code builds unchanged. Works on desktop and WASM.
option(ENABLE_DEBUG_UI "Build the Dear ImGui debug UI overlay" ON)

include(cmake/Tracy.cmake)

include(cmake/Testing.cmake)

include(cmake/GameTarget.cmake)

find_package(Angelscript CONFIG REQUIRED)
find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
if (ENABLE_DEBUG_UI)
    find_package(imgui CONFIG REQUIRED)
endif ()
find_package(glm CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# ── Shared dependency link list ──
set(GAME_LINK_LIBS
    raylib
    glfw
    $<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static>
    glm::glm-header-only
    nlohmann_json::nlohmann_json
    Angelscript::angelscript
    $<$<BOOL:${ENABLE_DEBUG_UI}>:imgui::imgui>
    Jolt::Jolt
    spdlog::spdlog
)

set(GAME_COMPILE_DEFS
    GLM_ENABLE_EXPERIMENTAL
    GLM_GTC_INLINE_NAMESPACE
    GLM_EXT_INLINE_NAMESPACE
    GLM_GTX_INLINE_NAMESPACE
    $<$<BOOL:${ENABLE_DEBUG_UI}>:ENGINE_DEBUG_UI>
)

set(GAME_COMPILE_OPTS
    $<$<AND:$<COMPILE_LANG_AND_ID:CXX,MSVC>,$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:/Zi>
    $<$<AND:$<NOT:$<COMPILE_LANG_AND_ID:CXX,MSVC>>,$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:-g>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive-;/W4;/w14322>
    $<$<CXX_COMPILER_ID:Clang>:-Wall;-Wpedantic;-Werror=return-type>
)

# ── engine_lib: pure engine infrastructure (no game semantics) ────────────────
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "src/engine/*.cpp")
add_library(engine_lib STATIC ${ENGINE_SOURCES})
target_include_directories(engine_lib PUBLIC src ${STB_INCLUDE_DIRS})
target_compile_definitions(engine_lib PUBLIC ${GAME_COMPILE_DEFS})
target_compile_options(engine_lib PRIVATE ${GAME_COMPILE_OPTS})
target_link_libraries(engine_lib PUBLIC ${GAME_LINK_LIBS})
setup_tracy(engine_lib)
# Game Targets link this name, so the same game CMake builds in-tree and against an installed framework.
add_library(game_framework::engine_lib ALIAS engine_lib)

# ── sandbox: engine demonstration scenes ─────────────────────────────────────
file(GLOB_RECURSE SANDBOX_SOURCES CONFIGURE_DEPENDS "src/game/*.cpp")
list(APPEND SANDBOX_SOURCES "src/main.cpp")

add_executable(sandbox ${SANDBOX_SOURCES})
target_include_directories(sandbox PRIVATE src)
setup_game_target(sandbox)

if (ENABLE_PROFILE_LOGGING)
    target_compile_definitions(sandbox PRIVATE PROFILE_LOGGING_ENABLED)
    message(STATUS "JSONL profile logging auto-start enabled")
endif ()

# ── scene equivalence test ────────────────────────────────────────────────────
if (BUILD_SCENE_EQUIVALENCE_TEST AND NOT EMSCRIPTEN)
    add_subdirectory(tests/scene_equivalence)
endif ()

# ── engine unit tests ──
if (BUILD_TESTING AND CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_subdirectory(tests/unit)
endif ()

# ── Game Targets (ADR-0006) ──────────────────────────────────────────────────
# Every games/<name>/CMakeLists.txt is a Game Target; adding one needs no edit here.
file(GLOB GAME_CMAKE_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/games/*/CMakeLists.txt")
foreach (game_cmake IN LISTS GAME_CMAKE_FILES)
    get_filename_component(game_dir "${game_cmake}" DIRECTORY)
    add_subdirectory("${game_dir}")
endforeach ()
