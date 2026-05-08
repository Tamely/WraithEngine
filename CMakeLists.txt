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
option(AXIOM_ENABLE_WEBRTC
       "Enable the macOS WebRTC transport integration seam" OFF)
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
