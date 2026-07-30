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

add_library(prismaticnative SHARED
    native_bridge.cpp)

target_link_libraries(prismaticnative
    PRIVATE prismatic_synthetic prismatic_core
            android log)
