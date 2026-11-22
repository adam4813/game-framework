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

# Translucent pass: back-to-front sort, and Translucent entities leaving the opaque passes for a depth-write-off pass
add_engine_test(translucent_pass_test
    translucent_pass_test.cpp
)

# UI Image: fit modes, atlas sub-rect and tint resolved into the draw list, under ScrollRect and Modal, and JSON
add_engine_test(ui_image_test
    ui_image_test.cpp
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

# Level loads give the same transforms and physics bodies deferred or undeferred, in any component order
add_engine_test(scene_load_order_test
    scene_load_order_test.cpp
)

# JSON field parsers and Level loading of malformed component values
add_engine_test(json_loading_test
    json_loading_test.cpp
)

# CharacterBody: landing, walking and the position-only teleport rule, headless
add_engine_test(character_body_test
    character_body_test.cpp
)

# Character replay: restoring a character's state and stepping it alone for one Tick, as a Tick would
add_engine_test(character_replay_test
    character_replay_test.cpp
)

# Path assets: an empty path keeps a runtime handle, a path acquires, and removal frees only what the registry loaded
add_engine_test(path_asset_test
    path_asset_test.cpp
)

# Line queue: a 3D line pushed from C++ or a script (DrawLine3D) is drawn in that frame's 3D pass, once, and never
# piles up
add_engine_test(line_queue_test
    line_queue_test.cpp
)

# Two physics worlds in one process: shared Jolt globals survive either teardown order; only a windowed world installs
# the debug renderer
add_engine_test(two_physics_worlds_test
    two_physics_worlds_test.cpp
)

# HeadlessPlatform: asset paths resolve against its directory, a world created without a Platform gets one, and
# saves are files in its save directory, written atomically
add_engine_test(headless_platform_test
    headless_platform_test.cpp
)

# File References: "@file:" strings resolve relative to their own file; missing files and cycles fail cleanly
add_engine_test(json_file_ref_test
    json_file_ref_test.cpp
)

# REST port: FLECS_REST_PORT overrides a world's given port unless the world is initialized to ignore it
add_engine_test(rest_port_override_test
    rest_port_override_test.cpp
)

# World logger: a world's named logger shares the default sinks; a world without one uses the default logger
add_engine_test(world_logger_test
    world_logger_test.cpp
)

# Fixed step: Ticks per Frame from the accumulator, the catch-up cap, A, B, A, B order, pause, run-once and alpha
add_engine_test(fixed_step_test
    fixed_step_test.cpp
)

# Raycast queries: closest hit and normal, sorted RaycastAll, layer masks, ignore lists, degenerate rays, inner bodies
add_engine_test(physics_raycast_test
    physics_raycast_test.cpp
)

# Raycast debug drawing: a hit's ray, hit marker and normal, a miss, RaycastAll's hits, persistence, disabled
add_engine_test(physics_ray_debug_test
    physics_ray_debug_test.cpp
)

# Raycast script bindings, headless: Raycast, RaycastFilter (ignore, layer mask) and RaycastAll's array of hits
add_engine_test(raycast_script_test
    raycast_script_test.cpp
)

# Calling a named script function from C++ (IScriptInstance::CallFunction): arguments, script state, overloads, an
# undefined function, an exception's message and line, and the function list with its parameter kinds
add_engine_test(script_call_function_test
    script_call_function_test.cpp
)

# EventHost: handlers checked against an event table, two scripts on one table, the call queue refusing calls made
# outside a handler, After() timers, and a throwing handler disabling only its own script
add_engine_test(script_event_host_test
    script_event_host_test.cpp
)

# Script lifecycle, headless: `self` is the host, a script that throws is disabled (OnDestroy skipped), and a string
# constant with quotes and backslashes compiles
add_engine_test(script_lifecycle_test
    script_lifecycle_test.cpp
)

# RigidBody / CollisionShape set after the body exists: motion type, mass and layer update in place, a static body
# turned dynamic or a new shape recreates it, and a repeated set changes nothing
add_engine_test(rigid_body_update_test
    rigid_body_update_test.cpp
)

# UIAnchor: anchored placement against the parent rect or canvas, pivot default, offset sign, Stack interplay
add_engine_test(ui_anchor_test
    ui_anchor_test.cpp
)

# UICapture: a UI-owned pointer click, drag or modal is withheld from gameplay by InputPoll
add_engine_test(ui_pointer_capture_test
    ui_pointer_capture_test.cpp
)

# UICapture.keyboard: a focused text field takes the keyboard from gameplay but still edits from RawInput
add_engine_test(ui_keyboard_capture_test
    ui_keyboard_capture_test.cpp
)
