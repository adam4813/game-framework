cmake_minimum_required(VERSION 3.30)
project(webgpu_spike LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(SPIKE_IMGUI "Draw imgui through its WebGPU backend" ON)
option(SPIKE_COPY_DXC "Copy dxcompiler.dll and dxil.dll beside the exe" ON)

find_package(Dawn CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

if (EMSCRIPTEN)
    add_executable(webgpu_spike spike.cpp platform_sdl.cpp platform_web.cpp)
    set(WEB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/web)
    set_target_properties(webgpu_spike PROPERTIES SUFFIX ".html"
        LINK_DEPENDS "${WEB_DIR}/shell.html;${WEB_DIR}/web_device.js")
    # Memory and stack as in cmake/GameTarget.cmake.
    target_link_options(webgpu_spike PRIVATE
        -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sMAXIMUM_MEMORY=134217728 -sSTACK_SIZE=512KB
        "SHELL:--shell-file ${WEB_DIR}/shell.html"
        "SHELL:--pre-js ${WEB_DIR}/web_device.js"
        "$<$<CONFIG:Debug>:-sASSERTIONS>"
        "$<$<NOT:$<CONFIG:Debug>>:-O3;--closure 1>")
else ()
    add_executable(webgpu_spike spike.cpp platform_sdl.cpp platform_native.cpp)
endif ()
target_link_libraries(webgpu_spike PRIVATE dawn::webgpu_dawn SDL3::SDL3)
if (MSVC)
    target_compile_options(webgpu_spike PRIVATE /W4)
    target_compile_definitions(webgpu_spike PRIVATE _CRT_SECURE_NO_WARNINGS)
endif ()
if (WIN32)
    target_link_libraries(webgpu_spike PRIVATE dxgi)
endif ()
if (SPIKE_IMGUI)
    find_package(imgui CONFIG REQUIRED)
    target_link_libraries(webgpu_spike PRIVATE imgui::imgui)
    target_compile_definitions(webgpu_spike PRIVATE SPIKE_IMGUI)
endif ()
if (WIN32 AND SPIKE_COPY_DXC)
    find_package(directx-dxc CONFIG REQUIRED)
    foreach (lib Microsoft::DirectXShaderCompiler Microsoft::DXIL)
        add_custom_command(TARGET webgpu_spike POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${lib}> $<TARGET_FILE_DIR:webgpu_spike>)
    endforeach ()
endif ()
