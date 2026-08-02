# SPDX-License-Identifier: GPL-3.0-or-later
# PRISMATIC — game platform CLI (library, ROM identity, mod profiles).
add_executable(prism main.cpp)
target_link_libraries(prism PRIVATE prismatic_core)
target_compile_features(prism PRIVATE cxx_std_20)
set_target_properties(prism PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
