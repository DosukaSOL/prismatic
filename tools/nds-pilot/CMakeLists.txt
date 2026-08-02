# SPDX-License-Identifier: GPL-3.0-or-later
# PRISMATIC — interactive NDS pilot (development tool, melonDS-backed).
add_executable(prismatic_nds_pilot main.cpp)
target_link_libraries(prismatic_nds_pilot PRIVATE prismatic_melonds prismatic_core)
