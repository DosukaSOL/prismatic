# SPDX-License-Identifier: GPL-3.0-or-later
# PRISMATIC synthetic (fixture) backend.
add_library(prismatic_synthetic STATIC
  synthetic_backend.cpp
)
target_include_directories(prismatic_synthetic PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(prismatic_synthetic PUBLIC prismatic_core)
