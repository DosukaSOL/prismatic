# SPDX-License-Identifier: GPL-3.0-or-later
# PRISMATIC test suite (CTest).
set(PRISMATIC_TEST_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})

function(prismatic_test name src)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE prismatic_core prismatic_synthetic)
  target_include_directories(${name} PRIVATE ${PRISMATIC_TEST_INCLUDE})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

prismatic_test(test_hash        unit/test_hash.cpp)
prismatic_test(test_json        unit/test_json.cpp)
prismatic_test(test_png         unit/test_png.cpp)
prismatic_test(test_materials   unit/test_materials.cpp)
prismatic_test(test_profile     unit/test_profile.cpp)
prismatic_test(test_mod_registry unit/test_mod_registry.cpp)
prismatic_test(test_pipeline    integration/test_pipeline.cpp)

# Shader compilation test (real GLSL -> SPIR-V via NDK glslc, if available).
add_test(NAME shader_compile
         COMMAND ${CMAKE_COMMAND} -E env
                 bash ${CMAKE_SOURCE_DIR}/scripts/test_rendering.sh ${CMAKE_BINARY_DIR}/shaders)
