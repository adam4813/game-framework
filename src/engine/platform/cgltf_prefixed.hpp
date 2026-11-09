#pragma once

// The engine's own copy of cgltf, with every public function renamed. Include this instead of <cgltf.h>; exactly one
// translation unit (physics/gltf_geometry.cpp) defines CGLTF_IMPLEMENTATION before including it.
//
// cgltf is header-only and has no way to give its functions internal linkage. Raylib compiles its own copy, and in the
// web build its symbols are external too (wasm-ld: duplicate symbol cgltf_parse). Prefixing every public function this
// copy defines keeps the two apart. If a cgltf upgrade adds a public function, the link names it as a duplicate: add
// it here.
#define cgltf_accessor_read_float engine_cgltf_accessor_read_float
#define cgltf_accessor_read_index engine_cgltf_accessor_read_index
#define cgltf_accessor_read_uint engine_cgltf_accessor_read_uint
#define cgltf_accessor_unpack_floats engine_cgltf_accessor_unpack_floats
#define cgltf_buffer_view_data engine_cgltf_buffer_view_data
#define cgltf_calc_size engine_cgltf_calc_size
#define cgltf_component_size engine_cgltf_component_size
#define cgltf_copy_extras_json engine_cgltf_copy_extras_json
#define cgltf_decode_string engine_cgltf_decode_string
#define cgltf_decode_uri engine_cgltf_decode_uri
#define cgltf_find_accessor engine_cgltf_find_accessor
#define cgltf_free engine_cgltf_free
#define cgltf_load_buffer_base64 engine_cgltf_load_buffer_base64
#define cgltf_load_buffers engine_cgltf_load_buffers
#define cgltf_node_transform_local engine_cgltf_node_transform_local
#define cgltf_node_transform_world engine_cgltf_node_transform_world
#define cgltf_num_components engine_cgltf_num_components
#define cgltf_parse engine_cgltf_parse
#define cgltf_parse_file engine_cgltf_parse_file
#define cgltf_validate engine_cgltf_validate
#define cgltf_accessor_unpack_indices engine_cgltf_accessor_unpack_indices
#define cgltf_accessor_index engine_cgltf_accessor_index
#define cgltf_animation_channel_index engine_cgltf_animation_channel_index
#define cgltf_animation_index engine_cgltf_animation_index
#define cgltf_animation_sampler_index engine_cgltf_animation_sampler_index
#define cgltf_buffer_index engine_cgltf_buffer_index
#define cgltf_buffer_view_index engine_cgltf_buffer_view_index
#define cgltf_camera_index engine_cgltf_camera_index
#define cgltf_image_index engine_cgltf_image_index
#define cgltf_light_index engine_cgltf_light_index
#define cgltf_material_index engine_cgltf_material_index
#define cgltf_mesh_index engine_cgltf_mesh_index
#define cgltf_node_index engine_cgltf_node_index
#define cgltf_sampler_index engine_cgltf_sampler_index
#define cgltf_scene_index engine_cgltf_scene_index
#define cgltf_skin_index engine_cgltf_skin_index
#define cgltf_texture_index engine_cgltf_texture_index
#include <cgltf.h>
