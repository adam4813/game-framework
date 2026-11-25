# Throwaway: measures extracting an unculled draw list (wayfinder ticket 09). Never merged.
add_executable(extraction_bench extraction_bench.cpp)
target_link_libraries(extraction_bench PRIVATE engine_lib)
if (EMSCRIPTEN)
    # Runs under node, no window, no preloaded assets.
    target_link_options(extraction_bench PRIVATE -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1MB
        -sUSE_GLFW=3 -sEXIT_RUNTIME=1)
    set_target_properties(extraction_bench PROPERTIES SUFFIX ".cjs")
endif ()
