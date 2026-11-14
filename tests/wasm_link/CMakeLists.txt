# engine_wasm_link: link-only check of the engine under Emscripten, built by CI's engine workflow in
# place of every Game Target. setup_game_target gives it the games' link flags (-s options, shell,
# --pre-js, --closure in Release); ROOT_ASSETS keeps its preload bundle to what the engine itself loads.
add_executable(engine_wasm_link main.cpp)
setup_game_target(engine_wasm_link ROOT_ASSETS shaders config)

# A static library only contributes the objects something references. Linking the whole archive makes
# every engine object resolve its symbols, so an undefined symbol anywhere in the engine fails here.
# A link option rather than $<LINK_LIBRARY:WHOLE_ARCHIVE,...>: setup_game_target already links engine_lib
# plainly, and CMake refuses one library linked both with and without a feature. wasm-ld resolves archives
# regardless of order, so the archive's dependencies listed later still resolve.
target_link_options(engine_wasm_link PRIVATE
    "LINKER:--whole-archive" "$<TARGET_FILE:engine_lib>" "LINKER:--no-whole-archive")
