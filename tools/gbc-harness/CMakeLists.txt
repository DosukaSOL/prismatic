# SPDX-License-Identifier: GPL-3.0-or-later
# PRISMATIC — Game Boy Color ROM smoke harness (host).
add_executable(prismatic_gbc_harness main.cpp)
target_link_libraries(prismatic_gbc_harness PRIVATE prismatic_core prismatic_gbc)
target_compile_features(prismatic_gbc_harness PRIVATE cxx_std_20)
set_target_properties(prismatic_gbc_harness PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
