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
project(prismatic LANGUAGES CXX VERSION 0.1.0)

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

message(STATUS "PRISMATIC configured: build type=${CMAKE_BUILD_TYPE}, compiler=${CMAKE_CXX_COMPILER_ID}")
