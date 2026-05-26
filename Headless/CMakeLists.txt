add_library(uSockets STATIC
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/bsd.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/context.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/loop.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/socket.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/udp.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/eventing/epoll_kqueue.c"
)

target_include_directories(uSockets PUBLIC
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src"
)

target_compile_definitions(uSockets PUBLIC LIBUS_NO_SSL)

add_library(uWebSocketsVendor INTERFACE)
target_include_directories(uWebSocketsVendor INTERFACE
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/src"
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src"
)
target_link_libraries(uWebSocketsVendor INTERFACE uSockets)

add_library(WraithNetworking STATIC
  RemoteViewportServer.cpp
  WraithNetworkingModule.cpp
)

target_include_directories(WraithNetworking PUBLIC
  "${CMAKE_SOURCE_DIR}/Headless"
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_link_libraries(WraithNetworking PUBLIC AxiomCore uWebSocketsVendor)

if(WIN32)
  target_link_libraries(WraithNetworking PUBLIC ws2_32 advapi32)
endif()

add_executable(AxiomHeadless
  AxiomHeadless.cpp
  HeadlessCommandProtocol.cpp
  HeadlessOverlayModule.cpp
  HostModules.cpp
  HeadlessSessionHost.cpp
  HeadlessSessionModule.cpp
  WebRtcSession.cpp
)

add_executable(AxiomRemoteViewportDevClient
  DevRemoteViewportClient.cpp
  HeadlessCommandProtocol.cpp
  HeadlessOverlayModule.cpp
  HostModules.cpp
  HeadlessSessionHost.cpp
  HeadlessSessionModule.cpp
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
  HeadlessCommandProtocol.cpp
  HeadlessOverlayModule.cpp
  HostModules.cpp
  HeadlessSessionHost.cpp
  HeadlessSessionModule.cpp
  WebRtcSession.cpp
)

add_executable(AxiomPackagedRuntime
  AxiomPackagedRuntime.cpp
  HeadlessOverlayModule.cpp
  HostModules.cpp
  PackagedRuntimeHost.cpp
  HeadlessSessionModule.cpp
)

target_include_directories(AxiomRemoteViewportServer PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_link_libraries(AxiomRemoteViewportServer PRIVATE AxiomCore WraithNetworking)
target_link_libraries(AxiomPackagedRuntime PRIVATE AxiomCore)

set_target_properties(AxiomPackagedRuntime PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Headless"
)

target_compile_definitions(AxiomCore PUBLIC
  AXIOM_PACKAGED_RUNTIME_BINARY_PATH="$<TARGET_FILE:AxiomPackagedRuntime>"
)
