# SPDX-License-Identifier: GPL-3.0-or-later
# PRISMATIC deterministic core library.
add_library(prismatic_core STATIC
  src/composite.cpp
  src/scene.cpp
  src/materials.cpp
  src/lighting.cpp
  src/environment.cpp
  src/camera.cpp
  src/profile.cpp
  src/presets.cpp
  src/renderer_software.cpp
  src/emulator_present.cpp
  src/voxel_diorama.cpp
  src/pipeline.cpp
  src/mod_registry.cpp
  src/game_profile.cpp
  src/game_lighting.cpp
  src/vcdiff.cpp
  src/game_library.cpp
  src/mod_packages.cpp
  src/nitrofs.cpp
)

target_include_directories(prismatic_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(prismatic_core PUBLIC cxx_std_20)
