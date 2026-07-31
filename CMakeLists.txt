# SPDX-License-Identifier: GPL-3.0-or-later
# PRISMATIC — top-level build.
#
# Desktop (host) build target set:
#   - prismatic_core         : deterministic, testable C++20 core (no GPU, no
#                              third-party deps)
#   - prismatic_synthetic    : deterministic DS-like backend (fixtures)
#   - prismatic_headless     : headless runner that produces PNGs + HTML report
#   - prismatic_tests        : CTest suite
#
# The Android app (app-android/) is built separately via Gradle + its own
# CMake invocation from the NDK; it is intentionally NOT part of this desktop
# tree so the host build stays dependency-light and fully testable.
cmake_minimum_required(VERSION 3.19)
project(prismatic LANGUAGES CXX VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Warnings-as-signal (not fatal, so the build is robust across compilers).
if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

enable_testing()

add_subdirectory(core)
add_subdirectory(emulation/synthetic)
add_subdirectory(tools/headless-runner)
add_subdirectory(tests)

# ---- Optional: real Nintendo DS backend (melonDS core) --------------------
# Off by default so the core test build stays dependency-light. When ON, this
# vendored melonDS core (third_party/melonDS submodule) is compiled and linked
# behind PRISMATIC's EmulatorAdapter so real DS ROMs boot and get enhanced.
option(PRISMATIC_ENABLE_MELONDS "Build the melonDS Nintendo DS backend" OFF)
if(PRISMATIC_ENABLE_MELONDS)
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/melonDS/src/NDS.h")
    message(FATAL_ERROR
      "PRISMATIC_ENABLE_MELONDS is ON but third_party/melonDS is empty. "
      "Run: git submodule update --init --recursive")
  endif()
  # melonDS build knobs: core only, no Qt/SDL frontend, no GL renderer, no GDB
  # stub, no vcpkg. The A64/x64 JIT is great on the Thor (Android arm64) but the
  # host Apple-Silicon JIT needs extra entitlements, so default it OFF on APPLE.
  set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
  set(ENABLE_OGLRENDERER OFF CACHE BOOL "" FORCE)
  set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
  set(USE_VCPKG OFF CACHE BOOL "" FORCE)
  if(APPLE AND NOT DEFINED ENABLE_JIT)
    set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
  endif()
  add_subdirectory(third_party/melonDS ${CMAKE_BINARY_DIR}/melonDS EXCLUDE_FROM_ALL)
  add_subdirectory(emulation/melonds)
  add_subdirectory(tools/nds-harness)
endif()

# ---- Optional: real Game Boy Color backend (SameBoy core) -----------------
# Off by default so the core test build stays dependency-light. When ON, the
# vendored SameBoy core (third_party/SameBoy submodule, MIT — GPL-compatible) is
# compiled behind PRISMATIC's EmulatorAdapter so real GB/GBC ROMs (e.g. Pokemon
# Crystal) boot and feed the tile-extruded voxel 2.5D renderer.
option(PRISMATIC_ENABLE_SAMEBOY "Build the SameBoy Game Boy Color backend" OFF)
if(PRISMATIC_ENABLE_SAMEBOY)
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/SameBoy/Core/gb.h")
    message(FATAL_ERROR
      "PRISMATIC_ENABLE_SAMEBOY is ON but third_party/SameBoy is empty. "
      "Run: git submodule update --init --recursive")
  endif()
  add_subdirectory(emulation/gbc)
  add_subdirectory(tools/gbc-harness)
endif()

# ---- Crystal recomp: data-driven voxel diorama renderer (core-only) --------
# Renders .pvx scene blobs (baked from the gitignored pokecrystal data) as 2.5D
# voxel dioramas. Only needs prismatic_core, so it builds in any configuration.
add_subdirectory(tools/crystal-recomp)

# ---- Mod manager: inspect/toggle the data-driven graphics & data mods -------
# Discovers mods/<id>/manifest.json (the built-in voxel diorama renderer ships
# as the reference graphics mod). Core-only, builds in any configuration.
add_subdirectory(tools/mod-manager)

message(STATUS "PRISMATIC configured: build type=${CMAKE_BUILD_TYPE}, compiler=${CMAKE_CXX_COMPILER_ID}")
