add_executable(AxiomHeadless
  AxiomHeadless.cpp
  HeadlessCommandProtocol.cpp
  HeadlessSessionLayer.cpp
)

target_include_directories(AxiomHeadless PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_link_libraries(AxiomHeadless PRIVATE AxiomCore)
