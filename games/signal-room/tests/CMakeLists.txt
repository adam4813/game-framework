add_signal_room_test(room_bootstrap_test
    room_harness.cpp
    room_bootstrap_test.cpp
)
target_include_directories(room_bootstrap_test PRIVATE "${CMAKE_SOURCE_DIR}/tests/scene_equivalence")
target_compile_definitions(room_bootstrap_test PRIVATE
    SIGNAL_ROOM_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets"
    SIGNAL_ROOM_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
