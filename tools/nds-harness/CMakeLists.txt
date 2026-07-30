# SPDX-License-Identifier: GPL-3.0-or-later
# PRISMATIC — Nintendo DS ROM smoke harness (host).
# Built only when PRISMATIC_ENABLE_MELONDS=ON (added from the root build).
add_executable(prismatic_nds_harness main.cpp)
target_link_libraries(prismatic_nds_harness PRIVATE prismatic_melonds prismatic_core)
