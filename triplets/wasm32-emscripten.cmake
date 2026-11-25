# vcpkg's own wasm32-emscripten triplet, with raylib built for OpenGL ES 3 (WebGL 2): the lit shaders read the clustered
# lights with texelFetch (ADR 0009). Used by the `wasm` CMake preset through VCPKG_OVERLAY_TRIPLETS, which shadows the
# triplet of the same name.
include("${VCPKG_ROOT_DIR}/triplets/community/wasm32-emscripten.cmake")

if (PORT STREQUAL "raylib")
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "-DOPENGL_VERSION=ES 3.0")
endif ()
