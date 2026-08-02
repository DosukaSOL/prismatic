# SPDX-License-Identifier: GPL-3.0-or-later
# PRISMATIC — differential parity harness (melonDS-backed).
add_executable(prismatic_parity main.cpp)
target_link_libraries(prismatic_parity PRIVATE prismatic_melonds prismatic_core)
