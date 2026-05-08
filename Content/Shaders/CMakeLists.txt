find_program(
  GLSL_VALIDATOR
  glslangValidator
  HINTS
    /usr/bin
    /usr/local/bin
    /opt/homebrew/bin
    $ENV{VULKAN_SDK}/bin
    $ENV{VULKAN_SDK}/Bin
    $ENV{VULKAN_SDK}/Bin32
)

if(NOT GLSL_VALIDATOR)
  message(FATAL_ERROR "glslangValidator was not found. Install it with `brew install glslang` or set VULKAN_SDK to a Vulkan SDK install.")
endif()

## Find all the shader files under the shaders folder
file(GLOB_RECURSE GLSL_SOURCE_FILES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/Content/Shaders/*.frag"
  "${CMAKE_SOURCE_DIR}/Content/Shaders/*.vert"
  "${CMAKE_SOURCE_DIR}/Content/Shaders/*.comp"
)

message(STATUS "Found shaders: ${GLSL_SOURCE_FILES}")

## Iterate each shader
foreach (GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${CMAKE_SOURCE_DIR}/Content/Shaders/${FILE_NAME}.spv")
  message(STATUS "${GLSL}")

  ## Execute glslang command to compile that specific shader
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
  Shaders
  DEPENDS ${SPIRV_BINARY_FILES}
)
