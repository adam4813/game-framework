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

# AnimationPlayer playback: loop wrap, clamp and stop, speed scaling, unbound players
add_engine_test(animation_playback_test
    animation_playback_test.cpp
)

# Point lights packed into LightParams: order, the 4-light cap and the dropped count
add_engine_test(point_light_packing_test
    point_light_packing_test.cpp
)

# CPU glTF loader behind mesh colliders: settles that cgltf links beside raylib's copy and applies node transforms
add_engine_test(gltf_geometry_test
    gltf_geometry_test.cpp
)

# glTF doubleSided flags per material, which raylib's LoadModel drops
add_engine_test(gltf_materials_test
    gltf_materials_test.cpp
)

# Render command batching: double-sided and culled draws split, tint doesn't
add_engine_test(render_batch_test
    render_batch_test.cpp
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

# Level loads give the same transforms and physics bodies deferred or undeferred, in any component order
add_engine_test(scene_load_order_test
    scene_load_order_test.cpp
)
target_include_directories(scene_load_order_test PRIVATE "${CMAKE_SOURCE_DIR}/tests/scene_equivalence")

# JSON field parsers and Level loading of malformed component values
add_engine_test(json_loading_test
    json_loading_test.cpp
)

# CharacterBody: landing, walking and the position-only teleport rule, headless
add_engine_test(character_body_test
    character_body_test.cpp
)
target_include_directories(character_body_test PRIVATE "${CMAKE_SOURCE_DIR}/tests/scene_equivalence")

# Path assets: an empty path keeps a runtime handle, a path acquires, and removal frees only what the registry loaded
add_engine_test(path_asset_test
    path_asset_test.cpp
)
target_include_directories(path_asset_test PRIVATE "${CMAKE_SOURCE_DIR}/tests/scene_equivalence")
