# Shared setup for every Game Target executable (ADR-0006): engine link, compile options, Tracy,
# the asset copy next to the binary, and the Emscripten link flags. Call right after add_executable.
# Reads GAME_COMPILE_OPTS from the root CMakeLists.txt.
function(setup_game_target target)
    target_compile_options(${target} PRIVATE ${GAME_COMPILE_OPTS})
    target_link_libraries(${target} PRIVATE engine_lib)
    setup_tracy(${target})

    if (NOT EMSCRIPTEN)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${target}>/assets
            COMMENT "Copying assets to ${target} build directory"
        )
        return()
    endif ()

    add_custom_command(TARGET ${target} PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        ${CMAKE_CURRENT_BINARY_DIR}/assets
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
        --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets
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
