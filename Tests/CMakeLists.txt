add_executable(AxiomTests
  LayerTests.cpp
  HeadlessProtocolTests.cpp
  ../Headless/HeadlessCommandProtocol.cpp
)

target_link_libraries(AxiomTests PRIVATE
  AxiomCore
  GTest::gtest_main
)

target_include_directories(AxiomTests PRIVATE
  "${CMAKE_SOURCE_DIR}/Headless"
)

gtest_discover_tests(AxiomTests)
