# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(prismatic_recomp_render render_scene.cpp)
target_link_libraries(prismatic_recomp_render PRIVATE prismatic_core)
target_compile_features(prismatic_recomp_render PRIVATE cxx_std_20)
set_target_properties(prismatic_recomp_render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
