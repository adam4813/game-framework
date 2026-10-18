add_signal_room_test(room_bootstrap_test
    room_harness.cpp
    room_bootstrap_test.cpp
)
add_signal_room_test(room_layout_test
    room_harness.cpp
    room_layout_test.cpp
)
foreach (test_target room_bootstrap_test room_layout_test)
    target_include_directories(${test_target} PRIVATE "${CMAKE_SOURCE_DIR}/tests/scene_equivalence")
    target_compile_definitions(${test_target} PRIVATE
        SIGNAL_ROOM_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        SIGNAL_ROOM_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    )
endforeach ()
