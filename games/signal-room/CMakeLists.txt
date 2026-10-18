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
target_compile_options(signal_room PRIVATE ${GAME_COMPILE_OPTS})
target_link_libraries(signal_room PRIVATE signal_room_lib)
setup_tracy(signal_room)

# ── Assets ───────────────────────────────────────────────────────────────────
# The engine needs its shaders (and the optional profiling filter) from the root assets; everything
# else comes from the game's own assets directory.
set(SIGNAL_ROOM_ENGINE_ASSETS shaders config)

if (EMSCRIPTEN)
    set(SIGNAL_ROOM_PRELOAD_FLAGS "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets")
    foreach (dir IN LISTS SIGNAL_ROOM_ENGINE_ASSETS)
        list(APPEND SIGNAL_ROOM_PRELOAD_FLAGS "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/assets/${dir}@/assets/${dir}")
    endforeach ()

    target_link_options(signal_room PRIVATE
        -sWASM=1
        -sUSE_GLFW=3
        -sFORCE_FILESYSTEM=1
        -sINITIAL_MEMORY=67108864
        -sMAXIMUM_MEMORY=134217728
        -sALLOW_MEMORY_GROWTH=1
        -sGL_ENABLE_GET_PROC_ADDRESS=1
        ${SIGNAL_ROOM_PRELOAD_FLAGS}
        -sSTACK_SIZE=512KB
        # SHELL: keeps each flag paired with its path; CMake would de-duplicate a bare repeated --pre-js.
        "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/tools/wasm_abort.js"
        "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/tools/wasm_audio_unlock.js"
        "$<$<CONFIG:Debug>:-sASSERTIONS -sSTACK_OVERFLOW_CHECK=2;-sGL_DEBUG=1>"
        "$<$<NOT:$<CONFIG:Debug>>:-O3;--closure 1>")
else ()
    foreach (dir IN LISTS SIGNAL_ROOM_ENGINE_ASSETS)
        add_custom_command(TARGET signal_room POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets/${dir}
            $<TARGET_FILE_DIR:signal_room>/assets/${dir}
            COMMENT "Copying engine ${dir} to signal_room build directory"
        )
    endforeach ()
    add_custom_command(TARGET signal_room POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:signal_room>/assets
        COMMENT "Copying assets to signal_room build directory"
    )
endif ()

# ── Tests ────────────────────────────────────────────────────────────────────
if (BUILD_TESTING AND NOT EMSCRIPTEN AND CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Mirrors add_engine_test: links the game library so tests exercise the code that ships.
    function(add_signal_room_test test_name)
        add_executable(${test_name} ${ARGN})
        target_link_libraries(${test_name} PRIVATE GTest::gtest GTest::gtest_main signal_room_lib)
        gtest_discover_tests(${test_name})
    endfunction()

    add_subdirectory(tests)
endif ()
