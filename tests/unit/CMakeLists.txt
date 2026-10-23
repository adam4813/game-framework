cmake_minimum_required(VERSION 3.30)

# Add sanity check test
add_engine_test(sanity_check_test
    sanity_check_test.cpp
)

add_engine_test(synth_dsp_test
    synth_dsp_test.cpp
)

add_engine_test(synth_trigger_test
    synth_trigger_test.cpp
)

# Orthographic/perspective camera projection matrix (no GPU required)
add_engine_test(camera_projection_test
    camera_projection_test.cpp
)

if (ENABLE_DEBUG_UI)
    add_engine_test(imgui_debug_ui_test
        imgui_debug_ui_test.cpp
    )
endif ()

# KeyCode names and the KeyOverride singleton the Remote API presses keys through
add_engine_test(key_override_test
    key_override_test.cpp
)
target_include_directories(key_override_test PRIVATE "${CMAKE_SOURCE_DIR}/tests/scene_equivalence")

# JSON field parsers and Level loading of malformed component values
add_engine_test(json_loading_test
    json_loading_test.cpp
)
