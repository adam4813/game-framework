# Shared setup for every Game Target executable (ADR-0006): engine link, compile options, Tracy,
# the asset copy next to the binary, and the Emscripten link flags. Call right after add_executable.
# Reads GAME_COMPILE_OPTS from the root CMakeLists.txt.
#
#   setup_game_target(<target> [ROOT_ASSETS <dir>...])
#
# Assets: the shared root assets/ is copied first, then the calling game's own assets/ folder (if it
# has one) is layered over it, so both resolve under the same runtime asset directory. ROOT_ASSETS
# limits the root copy to the listed top-level folders (e.g. `shaders config` for a game that
# brings all its own content and needs only the engine's).
function(setup_game_target target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "ROOT_ASSETS")

    target_compile_options(${target} PRIVATE ${GAME_COMPILE_OPTS})
    target_link_libraries(${target} PRIVATE game_framework::engine_lib)
    setup_tracy(${target})
    # The calling game's project() version; empty for targets defined in the root project.
    if (PROJECT_VERSION)
        target_compile_definitions(${target} PRIVATE GAME_VERSION="${PROJECT_VERSION}")
    endif ()

    # Copy commands with a <dest> placeholder, filled in per platform below.
    set(ASSET_COPY_COMMANDS)
    if (ARG_ROOT_ASSETS)
        foreach (dir IN LISTS ARG_ROOT_ASSETS)
            list(APPEND ASSET_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/assets/${dir} <dest>/${dir})
        endforeach ()
    else ()
        list(APPEND ASSET_COPY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_SOURCE_DIR}/assets <dest>)
    endif ()
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets AND NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        list(APPEND ASSET_COPY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/assets <dest>)
    endif ()

    if (NOT EMSCRIPTEN)
        # An always-run target rather than a POST_BUILD step: POST_BUILD runs only when the executable relinks, so an
        # edited level, shader or model stayed stale until some C++ changed. copy_directory_if_different keeps an
        # unchanged tree cheap.
        list(TRANSFORM ASSET_COPY_COMMANDS REPLACE "<dest>" "$<TARGET_FILE_DIR:${target}>/assets")
        add_custom_target(${target}_assets ALL
            ${ASSET_COPY_COMMANDS}
            COMMENT "Copying assets to ${target} build directory"
        )
        add_dependencies(${target} ${target}_assets)
        return()
    endif ()

    # Staged before link so --preload-file packs the merged tree; cleared first so a deleted source
    # asset doesn't linger in the bundle.
    set(STAGED_ASSETS ${CMAKE_CURRENT_BINARY_DIR}/assets)
    list(TRANSFORM ASSET_COPY_COMMANDS REPLACE "<dest>" "${STAGED_ASSETS}")
    add_custom_command(TARGET ${target} PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${STAGED_ASSETS}
        ${ASSET_COPY_COMMANDS}
        COMMENT "Copying assets for ${target} WASM build"
        VERBATIM
    )

    set(INITIAL_MEMORY_BYTES 67108864)
    set(MAXIMUM_MEMORY_BYTES 134217728)

    set(SHELL_FILE ${CMAKE_SOURCE_DIR}/tools/wasm_shell.html)
    set_target_properties(${target} PROPERTIES SUFFIX ".html" LINK_DEPENDS ${SHELL_FILE})
    target_link_options(${target} PRIVATE
        -sWASM=1
        -sUSE_GLFW=3
        -sFORCE_FILESYSTEM=1
        -sINITIAL_MEMORY=${INITIAL_MEMORY_BYTES}
        -sMAXIMUM_MEMORY=${MAXIMUM_MEMORY_BYTES}
        -sALLOW_MEMORY_GROWTH=1
        -sGL_ENABLE_GET_PROC_ADDRESS=1
        --preload-file ${STAGED_ASSETS}@/assets
        # Canvas-only page so raylib's window-sized canvas fits an itch.io iframe without clipping.
        "SHELL:--shell-file ${SHELL_FILE}"
        -sSTACK_SIZE=512KB
        # SHELL: keeps each flag paired with its path; CMake would de-duplicate a bare repeated --pre-js.
        "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/tools/wasm_abort.js"
        "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/tools/wasm_audio_unlock.js"
        "$<$<CONFIG:Debug>:-sASSERTIONS -sSTACK_OVERFLOW_CHECK=2;-sGL_DEBUG=1>"
        "$<$<NOT:$<CONFIG:Debug>>:-O3;--closure 1>")

    message(STATUS "Emscripten configuration applied to ${target}")
    message(STATUS "Memory: Initial ${INITIAL_MEMORY_BYTES} bytes, Maximum ${MAXIMUM_MEMORY_BYTES} bytes")
endfunction()
