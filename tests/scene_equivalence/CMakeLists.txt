add_executable(scene_equivalence_test
    main.cpp
)

target_include_directories(scene_equivalence_test PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${STB_INCLUDE_DIRS}"
)

target_compile_definitions(scene_equivalence_test PUBLIC
    GLM_ENABLE_EXPERIMENTAL GLM_GTC_INLINE_NAMESPACE GLM_EXT_INLINE_NAMESPACE GLM_GTX_INLINE_NAMESPACE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_WARN
)

target_link_libraries(scene_equivalence_test PRIVATE engine_lib)
