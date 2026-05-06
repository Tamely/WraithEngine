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
  RemoteViewportBrowserAssets.cpp
  RemoteViewportServer.cpp
  HeadlessCommandProtocol.cpp
  HeadlessSessionHost.cpp
  HeadlessSessionLayer.cpp
  WebRtcSession.cpp
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

if(WIN32)
  target_link_libraries(AxiomRemoteViewportServer PRIVATE ws2_32)
  target_link_libraries(AxiomRemoteViewportServer PRIVATE advapi32)
endif()
