# Profiling triplet: inherits the standard x64-windows settings and adds FLECS_PERF_TRACE so
# the Flecs library is compiled with performance-trace hooks enabled (perf_trace_push_ /
# perf_trace_pop_). Used by the desktop-profile CMake preset.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_C_FLAGS   "${VCPKG_C_FLAGS}   -DFLECS_PERF_TRACE")
set(VCPKG_CXX_FLAGS "${VCPKG_CXX_FLAGS} -DFLECS_PERF_TRACE")
