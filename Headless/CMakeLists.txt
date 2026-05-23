add_executable(AxiomHeadless
  AxiomHeadless.cpp
  HeadlessCommandProtocol.cpp
  HeadlessSessionHost.cpp
  HeadlessSessionLayer.cpp
  WebRtcSession.cpp
)

add_executable(AxiomRemoteViewportDevClient
  DevRemoteViewportClient.cpp
  HeadlessCommandProtocol.cpp
  HeadlessSessionHost.cpp
  HeadlessSessionLayer.cpp
  WebRtcSession.cpp
)

target_include_directories(AxiomHeadless PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_include_directories(AxiomRemoteViewportDevClient PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_link_libraries(AxiomHeadless PRIVATE AxiomCore)
target_link_libraries(AxiomRemoteViewportDevClient PRIVATE AxiomCore)

add_executable(AxiomRemoteViewportServer
  AxiomRemoteViewportServer.cpp
  RemoteViewportServer.cpp
  HeadlessCommandProtocol.cpp
  HeadlessSessionHost.cpp
  HeadlessSessionLayer.cpp
  WebRtcSession.cpp
)

add_executable(AxiomPackagedRuntime
  AxiomPackagedRuntime.cpp
  PackagedRuntimeHost.cpp
  HeadlessSessionLayer.cpp
)

if(APPLE)
  target_sources(AxiomHeadless PRIVATE
    MacOSWebRtcSession.mm
  )
  target_sources(AxiomRemoteViewportDevClient PRIVATE
    MacOSWebRtcSession.mm
  )
  target_sources(AxiomRemoteViewportServer PRIVATE
    MacOSWebRtcSession.mm
  )
endif()

target_include_directories(AxiomRemoteViewportServer PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_link_libraries(AxiomRemoteViewportServer PRIVATE AxiomCore)
target_link_libraries(AxiomPackagedRuntime PRIVATE AxiomCore)

set_target_properties(AxiomPackagedRuntime PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Headless"
)

target_compile_definitions(AxiomCore PUBLIC
  AXIOM_PACKAGED_RUNTIME_BINARY_PATH="$<TARGET_FILE:AxiomPackagedRuntime>"
)

if(WIN32)
  target_link_libraries(AxiomRemoteViewportServer PRIVATE ws2_32)
  target_link_libraries(AxiomRemoteViewportServer PRIVATE advapi32)
endif()
