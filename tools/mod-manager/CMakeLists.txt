# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(prismatic_mod_manager main.cpp)
target_link_libraries(prismatic_mod_manager PRIVATE prismatic_core)
target_compile_features(prismatic_mod_manager PRIVATE cxx_std_20)
set_target_properties(prismatic_mod_manager PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    OUTPUT_NAME prismatic_mod_manager)
