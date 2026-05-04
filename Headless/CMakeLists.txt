add_executable(AxiomHeadless
  AxiomHeadless.cpp
  HeadlessCommandProtocol.cpp
  HeadlessSessionHost.cpp
  HeadlessSessionLayer.cpp
)

add_executable(AxiomRemoteViewportDevClient
  DevRemoteViewportClient.cpp
  HeadlessCommandProtocol.cpp
  HeadlessSessionHost.cpp
  HeadlessSessionLayer.cpp
)

target_include_directories(AxiomHeadless PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_include_directories(AxiomRemoteViewportDevClient PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_link_libraries(AxiomHeadless PRIVATE AxiomCore)
target_link_libraries(AxiomRemoteViewportDevClient PRIVATE AxiomCore)
