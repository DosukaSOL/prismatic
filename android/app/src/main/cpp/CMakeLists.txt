# SPDX-License-Identifier: GPL-3.0-or-later
# Native library for the PRISMATIC Android app.
# Reuses the SAME first-party C++ core (prismatic_core) and the synthetic DS
# backend (prismatic_synthetic) that are built and tested on desktop, so the
# on-device pipeline is identical to the validated one.
cmake_minimum_required(VERSION 3.22.1)
project(prismaticnative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Repository root: cpp -> main -> src -> app -> android -> ROOT
get_filename_component(PRISMATIC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

add_subdirectory(${PRISMATIC_ROOT}/core                 ${CMAKE_BINARY_DIR}/core_build)
add_subdirectory(${PRISMATIC_ROOT}/emulation/synthetic  ${CMAKE_BINARY_DIR}/synthetic_build)

# ---- Real Nintendo DS backend (melonDS core) ------------------------------
# The Thor is arm64, so enable the A64 JIT recompiler for full-speed emulation.
# Core only: no Qt/SDL frontend, no OpenGL renderer (we use the software
# renderer + PRISMATIC's own shading), no GDB stub, no vcpkg.
set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_OGLRENDERER OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
set(USE_VCPKG OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT ON CACHE BOOL "" FORCE)
add_subdirectory(${PRISMATIC_ROOT}/third_party/melonDS   ${CMAKE_BINARY_DIR}/melonDS_build EXCLUDE_FROM_ALL)
add_subdirectory(${PRISMATIC_ROOT}/emulation/melonds     ${CMAKE_BINARY_DIR}/melonds_build)

add_library(prismaticnative SHARED
    native_bridge.cpp)

target_link_libraries(prismaticnative
    PRIVATE prismatic_melonds prismatic_synthetic prismatic_core
            android log)
