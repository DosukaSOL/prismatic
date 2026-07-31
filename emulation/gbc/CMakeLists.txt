# SPDX-License-Identifier: GPL-3.0-or-later
# PRISMATIC — Game Boy / Game Boy Color backend (SameBoy core).
#
# Builds the vendored SameBoy Core (third_party/SameBoy, MIT) as a static C
# library, plus the PRISMATIC<->SameBoy glue (EmulatorAdapter that exposes the
# real framebuffer AND the structured BG tilemap/tiles/palettes/OAM needed for
# the tile-extruded voxel 2.5D). Enabled from the root build via
# -DPRISMATIC_ENABLE_SAMEBOY=ON.

set(SAMEBOY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/SameBoy)

# The vendored SameBoy Core is C; the root project only enables CXX, so turn on
# C here (scoped to when the SameBoy backend is built).
enable_language(C)

# ---- SameBoy Core (portable C99) ------------------------------------------
file(GLOB SAMEBOY_CORE_SOURCES ${SAMEBOY_DIR}/Core/*.c)
add_library(sameboy_core STATIC ${SAMEBOY_CORE_SOURCES})

target_include_directories(sameboy_core PUBLIC ${SAMEBOY_DIR} ${SAMEBOY_DIR}/Core)

# Match SameBoy's own Core build: internal access, C11 + GNU extensions.
target_compile_definitions(sameboy_core PRIVATE
  GB_INTERNAL
  _GNU_SOURCE
  _USE_MATH_DEFINES
  "GB_VERSION=\"1.0.3\""
  "GB_COPYRIGHT_YEAR=\"2025\"")

set_target_properties(sameboy_core PROPERTIES C_STANDARD 11 C_EXTENSIONS ON
                      POSITION_INDEPENDENT_CODE ON)

# SameBoy's Core is warning-clean only with its own -Wno set; silence here so it
# never breaks PRISMATIC's -Werror-free but noisy host build.
if(NOT MSVC)
  target_compile_options(sameboy_core PRIVATE
    -w -Wno-multichar -Wno-unused-result -Wno-format-truncation)
endif()

# ---- PRISMATIC GBC adapter -------------------------------------------------
# sameboy_shim.c is compiled as C and is the ONLY translation unit that includes
# SameBoy's gb.h; sameboy_backend.cpp (C++) talks only to the shim's C ABI.
add_library(prismatic_gbc STATIC sameboy_backend.cpp sameboy_shim.c)
target_include_directories(prismatic_gbc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(prismatic_gbc PUBLIC prismatic_core PRIVATE sameboy_core)
target_compile_features(prismatic_gbc PUBLIC cxx_std_20)
set_source_files_properties(sameboy_shim.c PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
