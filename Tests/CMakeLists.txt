add_executable(AxiomTests
  LayerTests.cpp
)

target_link_libraries(AxiomTests PRIVATE
  AxiomCore
  GTest::gtest_main
)

gtest_discover_tests(AxiomTests)
