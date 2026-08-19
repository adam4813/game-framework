cmake_minimum_required(VERSION 3.30)
project(game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src ${STB_INCLUDE_DIRS})
target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_ENABLE_EXPERIMENTAL GLM_GTC_INLINE_NAMESPACE GLM_EXT_INLINE_NAMESPACE GLM_GTX_INLINE_NAMESPACE)
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:/Zi>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:-g>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
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

if (EMSCRIPTEN)
    # Preload assets for WASM
    # Copy assets for WASM build
    add_custom_command(TARGET ${PROJECT_NAME} PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        ${CMAKE_CURRENT_BINARY_DIR}/assets
        COMMENT "Copying assets for WASM build"
        VERBATIM
    )
else ()
    # Copy assets to build directory for native builds
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
        COMMENT "Copying assets to build directory"
    )
endif ()

# Platform-specific configuration
if (EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    # Set memory limits
    set(INITIAL_MEMORY_BYTES 67108864) # 64MB initial memory
    set(MAXIMUM_MEMORY_BYTES 134217728) # 128MB maximum memory

    target_link_options(
        ${PROJECT_NAME}
        PRIVATE
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
        # Debug vs Release specific flags
        "$<$<CONFIG:Debug>:-sASSERTIONS -sSTACK_OVERFLOW_CHECK=2;-sGL_DEBUG=1>"
        "$<$<NOT:$<CONFIG:Debug>>:-O3;--closure 1>")

    message(STATUS "Emscripten configuration applied")
    message(
        STATUS
        "Memory: Initial ${INITIAL_MEMORY_BYTES} bytes, Maximum ${MAXIMUM_MEMORY_BYTES} bytes"
    )
endif ()
