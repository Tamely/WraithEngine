cmake_minimum_required(VERSION 3.10)
project(AxiomEngine VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTING "Build tests" OFF)

add_subdirectory(ThirdParty/spdlog)
add_subdirectory(ThirdParty/glfw)
add_subdirectory(ThirdParty/fastgltf)
add_subdirectory(ThirdParty/glm/glm)

add_subdirectory(Content/Shaders)
add_subdirectory(Axiom)
add_subdirectory(Editor)

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
