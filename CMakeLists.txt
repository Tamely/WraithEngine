cmake_minimum_required(VERSION 3.10)
project(AxiomEngine VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTING "Build tests" OFF)
option(AXIOM_ENABLE_SCRIPTING
       "Enable the Coral C# scripting host" OFF)
option(AXIOM_SCRIPTING_WATCH
       "Auto-reload user scripts when the assembly changes on disk (macOS kqueue)" OFF)
set(AXIOM_SCRIPTING_TRUST_DEFAULT "Restricted" CACHE STRING
    "Default trust profile for user scripts: Restricted (hosted) or Trusted (local)")
set_property(CACHE AXIOM_SCRIPTING_TRUST_DEFAULT PROPERTY STRINGS "Restricted" "Trusted")
option(AXIOM_ENABLE_WEBRTC
       "Enable the macOS WebRTC transport integration seam" OFF)
option(AXIOM_ENABLE_PHYSICS
       "Enable the JoltPhysics runtime simulation seam" ON)
set(AXIOM_WEBRTC_FRAMEWORK_PATH "" CACHE PATH
    "Optional path to a locally built WebRTC.framework for macOS WebRTC integration")
set(AXIOM_WEBRTC_LIBRARY_PATH "" CACHE FILEPATH
    "Optional path to a locally built libwebrtc binary when not using WebRTC.framework")
set(AXIOM_WEBRTC_INCLUDE_DIR "" CACHE PATH
    "Optional include directory for a locally built libwebrtc binary")

add_subdirectory(ThirdParty/spdlog)
add_subdirectory(ThirdParty/glfw)
add_subdirectory(ThirdParty/fastgltf)
add_subdirectory(ThirdParty/glm/glm)

include(FetchContent)
FetchContent_Declare(
  assimp
  URL https://github.com/assimp/assimp/archive/refs/tags/v5.4.3.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(ASSIMP_BUILD_TESTS            OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS     OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT              ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER     ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER     ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB             OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS     OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL                OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

if(AXIOM_ENABLE_PHYSICS)
  set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
  set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
  set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
  set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
  set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
  set(TARGET_TEST_FRAMEWORK OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.5.0
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(JoltPhysics)
  FetchContent_GetProperties(JoltPhysics
    SOURCE_DIR AXIOM_JOLT_SOURCE_DIR
    BINARY_DIR AXIOM_JOLT_BINARY_DIR)
  if(NOT TARGET Jolt)
    add_subdirectory("${AXIOM_JOLT_SOURCE_DIR}/Build" "${AXIOM_JOLT_BINARY_DIR}")
  endif()
endif()

if(AXIOM_ENABLE_SCRIPTING)
  include(cmake/CoralNative.cmake)
endif()

add_subdirectory(Content/Shaders)
add_subdirectory(Axiom)
add_subdirectory(Editor)
add_subdirectory(Headless)

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  include(GoogleTest)

  add_subdirectory(Tests)
endif()
