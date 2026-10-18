cmake_minimum_required(VERSION 3.30)

# Add sanity check test
add_engine_test(sanity_check_test
    sanity_check_test.cpp
)

if (ENABLE_DEBUG_UI)
    add_engine_test(imgui_debug_ui_test
        imgui_debug_ui_test.cpp
    )
endif ()
