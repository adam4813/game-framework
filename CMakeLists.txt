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

include(cmake/Tracy.cmake)

include(cmake/Testing.cmake)

find_package(Angelscript CONFIG REQUIRED)
find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
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
    imgui::imgui
    Jolt::Jolt
    spdlog::spdlog
)

set(GAME_COMPILE_DEFS
    GLM_ENABLE_EXPERIMENTAL
    GLM_GTC_INLINE_NAMESPACE
    GLM_EXT_INLINE_NAMESPACE
    GLM_GTX_INLINE_NAMESPACE
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

# ── sandbox: engine demonstration scenes ─────────────────────────────────────
file(GLOB_RECURSE SANDBOX_SOURCES CONFIGURE_DEPENDS "src/game/*.cpp")
list(APPEND SANDBOX_SOURCES "src/main.cpp")

add_executable(sandbox ${SANDBOX_SOURCES})
target_include_directories(sandbox PRIVATE src)
target_compile_options(sandbox PRIVATE ${GAME_COMPILE_OPTS})
target_link_libraries(sandbox PRIVATE engine_lib)
setup_tracy(sandbox)

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

# ── Assets: sandbox ──────────────────────────────────────────────────────────
if (EMSCRIPTEN)
    add_custom_command(TARGET sandbox PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        ${CMAKE_CURRENT_BINARY_DIR}/assets
        COMMENT "Copying assets for sandbox WASM build"
        VERBATIM
    )
else ()
    add_custom_command(TARGET sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:sandbox>/assets
        COMMENT "Copying assets to sandbox build directory"
    )
endif ()

# ── Platform-specific configuration: sandbox WASM ────────────────────────────
if (EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    set(INITIAL_MEMORY_BYTES 67108864)
    set(MAXIMUM_MEMORY_BYTES 134217728)

    target_link_options(sandbox PRIVATE
        -sWASM=1
        -sUSE_GLFW=3
        -sFORCE_FILESYSTEM=1
        -sINITIAL_MEMORY=${INITIAL_MEMORY_BYTES}
        -sMAXIMUM_MEMORY=${MAXIMUM_MEMORY_BYTES}
        -sALLOW_MEMORY_GROWTH=1
        -sGL_ENABLE_GET_PROC_ADDRESS=1
        --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets
        -sSTACK_SIZE=512KB
        --pre-js ${CMAKE_SOURCE_DIR}/tools/wasm_abort.js
        "$<$<CONFIG:Debug>:-sASSERTIONS -sSTACK_OVERFLOW_CHECK=2;-sGL_DEBUG=1>"
        "$<$<NOT:$<CONFIG:Debug>>:-O3;--closure 1>")

    message(STATUS "Emscripten configuration applied")
    message(
        STATUS
        "Memory: Initial ${INITIAL_MEMORY_BYTES} bytes, Maximum ${MAXIMUM_MEMORY_BYTES} bytes"
    )
endif ()
