# SPDX-License-Identifier: GPL-3.0-or-later
# PRISMATIC — melonDS Nintendo DS backend.
#
# Builds the PRISMATIC<->melonDS glue (Platform implementation + EmulatorAdapter)
# and links it against the vendored melonDS `core` static library. Enabled from
# the root build via -DPRISMATIC_ENABLE_MELONDS=ON.
add_library(prismatic_melonds STATIC
  melon_platform.cpp
  nds_adapter.cpp
)

target_include_directories(prismatic_melonds
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  # melonDS core headers — path is relative to this file so it resolves the same
  # in the desktop root build and the Android app build (different CMAKE_SOURCE_DIR).
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/melonDS/src
)

# prismatic_core = enhancement pipeline; core = melonDS DS emulation core.
target_link_libraries(prismatic_melonds
  PUBLIC prismatic_core
  PRIVATE core
)

target_compile_features(prismatic_melonds PUBLIC cxx_std_20)
