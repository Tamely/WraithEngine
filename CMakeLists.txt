cmake_minimum_required(VERSION 3.10)
project(AxiomEngine VERSION 0.1)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

add_subdirectory(ThirdParty/spdlog)
add_subdirectory(ThirdParty/glfw)
add_subdirectory(ThirdParty/fastgltf)
add_subdirectory(ThirdParty/glm/glm)

add_subdirectory(Axiom)
add_subdirectory(Editor)
add_subdirectory(Tests)

add_subdirectory(Content/Shaders)
