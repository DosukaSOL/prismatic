# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(prismatic_headless main.cpp)
target_link_libraries(prismatic_headless PRIVATE prismatic_core prismatic_synthetic)
