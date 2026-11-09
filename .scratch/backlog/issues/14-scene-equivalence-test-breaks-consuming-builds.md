# Fix: scene_equivalence_test breaks the build of any game that fetches the Framework

Status: resolved (branch fix/scene-equivalence-consumer-build)
Source: Trillek build failure against Framework `main`, 2026-11-09

## What happened

A game repo that fetches the Framework (Trillek) fails to build. In the root `CMakeLists.txt`,
`BUILD_SCENE_EQUIVALENCE_TEST` defaults to `ON` and is not limited to the Framework's own build. So
`add_subdirectory(tests/scene_equivalence)` runs in the consumer's configure. Its include directories use
`${CMAKE_SOURCE_DIR}/src` and `${CMAKE_SOURCE_DIR}/tests/support`, and `CMAKE_SOURCE_DIR` is the consuming repo's root,
so the includes don't resolve.

Workaround (not committed): `-DBUILD_SCENE_EQUIVALENCE_TEST=OFF` in the consumer's local build cache.

## Cause

Two separate faults:

1. The option defaults to `ON`, so every consumer pays for a test that only the Framework needs.
2. `tests/scene_equivalence/CMakeLists.txt` resolves paths against `CMAKE_SOURCE_DIR` instead of
   `PROJECT_SOURCE_DIR`, so it only works when the Framework is the top-level project. (The unit tests are guarded with
   `CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME`; this target has no such guard.)

## Fix

- `CMakeLists.txt`: default `BUILD_SCENE_EQUIVALENCE_TEST` to `${PROJECT_IS_TOP_LEVEL}`. No preset change is needed, and
  a consumer cannot get it by accident. (A preset-only `ON` was considered; this is more robust.)
- `tests/scene_equivalence/CMakeLists.txt`: replace `CMAKE_SOURCE_DIR` with `PROJECT_SOURCE_DIR` so the target is
  correct if someone turns the option on from a consumer.
- Grep the other `tests/` subdirectories for the same `CMAKE_SOURCE_DIR` pattern and fix any that are reachable from a
  consumer build.
- Docs that assume the old default: README "Scene Equivalence Test" and `.github/skills/create-module/SKILL.md` §8 build
  it from the `desktop` preset. Keep them accurate after the change.

## Done when

- [ ] Configuring a throwaway consumer project that `FetchContent`s the Framework succeeds with no
      `BUILD_SCENE_EQUIVALENCE_TEST` override, and does not define the `scene_equivalence_test` target.
- [ ] `cmake --preset desktop-test`, `cmake --build --preset desktop-test-release` and
      `ctest --preset desktop-test-release` build and run `scene_equivalence` as before; the test count is unchanged.
- [ ] `cmake --preset wasm` still configures.
- [ ] `macos-intel-test` still gets the target (it inherits `desktop`).
