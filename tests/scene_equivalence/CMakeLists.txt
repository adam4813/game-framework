file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/engine/*.cpp")

add_executable(scene_equivalence_test
    main.cpp
    ${ENGINE_SOURCES}
)

target_include_directories(scene_equivalence_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
    "${STB_INCLUDE_DIRS}"
)

target_compile_definitions(scene_equivalence_test PUBLIC
    GLM_ENABLE_EXPERIMENTAL GLM_GTC_INLINE_NAMESPACE GLM_EXT_INLINE_NAMESPACE GLM_GTX_INLINE_NAMESPACE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_WARN
)

target_link_libraries(scene_equivalence_test PRIVATE
    raylib glfw
    $<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static>
    glm::glm-header-only nlohmann_json::nlohmann_json
    Angelscript::angelscript imgui::imgui Jolt::Jolt spdlog::spdlog
)
