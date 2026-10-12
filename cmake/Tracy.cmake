# cmake/Tracy.cmake
# Provides setup_tracy(target) — links Tracy, sets TRACY_ENABLE, and registers the run-profiled
# convenience target that starts the Tracy GUI before launching the game.
#
# Usage (from CMakeLists.txt, after add_executable):
#   include(cmake/Tracy.cmake)
#   setup_tracy(${PROJECT_NAME})

function(setup_tracy GAME_TARGET)
    if (NOT ENABLE_TRACY OR EMSCRIPTEN OR NOT TARGET ${GAME_TARGET})
        return()
    endif ()

    find_package(Tracy CONFIG REQUIRED)

    target_link_libraries(${GAME_TARGET} PRIVATE Tracy::TracyClient)
    target_compile_definitions(${GAME_TARGET} PRIVATE TRACY_ENABLE)
    message(STATUS "Tracy profiler integration enabled for target '${GAME_TARGET}'")

    # Locate the Tracy GUI executable installed by vcpkg alongside the client library.
    set(TRACY_PROFILER_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/tracy")

    if (WIN32)
        set(TRACY_PROFILER_EXE "${TRACY_PROFILER_DIR}/tracy-profiler.exe")
    else ()
        set(TRACY_PROFILER_EXE "${TRACY_PROFILER_DIR}/tracy-profiler")
    endif ()

    if (NOT EXISTS "${TRACY_PROFILER_EXE}")
        message(WARNING
            "Tracy profiler GUI not found at ${TRACY_PROFILER_EXE}. "
            "The 'run-profiled' target will not launch it. "
            "Ensure the tracy vcpkg package includes the 'ui' feature."
        )
    endif ()

    # Only create run-profiled for executable targets; static/shared libraries have no entry point.
    get_target_property(_target_type ${GAME_TARGET} TYPE)
    if (NOT TARGET run-profiled AND _target_type STREQUAL "EXECUTABLE")
        # Cross-platform background launch: Windows uses 'start' so the GUI doesn't block;
        # macOS/Linux fork it via a shell.
        if (WIN32)
            set(LAUNCH_TRACY cmd /c start "" "${TRACY_PROFILER_EXE}")
        else ()
            set(LAUNCH_TRACY sh -c "\"${TRACY_PROFILER_EXE}\" &")
        endif ()

        # run-profiled: starts the Tracy GUI in the background, then launches the game so Tracy is
        # already listening on port 8086 before the game connects.
        add_custom_target(run-profiled
            COMMAND ${LAUNCH_TRACY}
            COMMAND $<TARGET_FILE:${GAME_TARGET}>
            WORKING_DIRECTORY $<TARGET_FILE_DIR:${GAME_TARGET}>
            COMMENT "Starting Tracy profiler GUI, then launching '${GAME_TARGET}'..."
            VERBATIM
            DEPENDS ${GAME_TARGET}
        )
    endif ()
endfunction()
