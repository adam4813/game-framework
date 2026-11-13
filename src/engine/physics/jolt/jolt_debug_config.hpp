#pragma once

// ENGINE_PHYSICS_DEBUG_RENDERER gates the engine's Jolt debug renderer (the adapter that draws bodies through the
// Platform, and the systems that drive it). It's on when Jolt was built with its debug renderer (JPH_DEBUG_RENDERER,
// the vcpkg `debugrenderer` feature, absent on Emscripten) and the ENABLE_PHYSICS_DEBUG_RENDERER CMake option is ON.
// Turning the option OFF strips the engine side only: JPH_DEBUG_RENDERER changes the layout of Jolt's own classes, so
// it has to match the prebuilt library and can't be undefined here.
#if defined(JPH_DEBUG_RENDERER) && !defined(ENGINE_NO_PHYSICS_DEBUG_RENDERER)
#define ENGINE_PHYSICS_DEBUG_RENDERER 1
#endif
