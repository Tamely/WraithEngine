add_library(uSockets STATIC
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/bsd.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/context.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/loop.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/socket.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/udp.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/uWebSockets/uSockets/src/eventing/epoll_kqueue.c"
)

axiom_apply_performance_options(uSockets)

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
  RemoteViewportGizmoController.cpp
  RemoteViewportGridSnap.cpp
  RemoteViewportHttpRouter.cpp
  RemoteViewportPresence.cpp
  RemoteViewportServer.cpp
  RemoteViewportWebRtcSessionManager.cpp
  RemoteViewportWebSocketDispatch.cpp
  WraithNetworkingModule.cpp
)

axiom_apply_performance_options(WraithNetworking)

target_include_directories(WraithNetworking PUBLIC
  "${CMAKE_SOURCE_DIR}/Headless"
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_link_libraries(WraithNetworking PUBLIC AxiomCore uWebSocketsVendor)
target_link_libraries(WraithNetworking PRIVATE AxiomHAL)
target_link_libraries(WraithNetworking PUBLIC
  AxiomNet
  AxiomScene
  AxiomRenderer
)
if(TARGET AxiomScripting)
  target_link_libraries(WraithNetworking PUBLIC AxiomScripting)
endif()

target_compile_definitions(WraithNetworking PRIVATE
  AXIOM_WITH_PHYSICS=$<IF:$<BOOL:${AXIOM_ENABLE_PHYSICS}>,1,0>
  AXIOM_WITH_SCRIPTING=$<IF:$<BOOL:${AXIOM_ENABLE_SCRIPTING}>,1,0>
)

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

axiom_apply_performance_options(AxiomHeadless)

add_executable(AxiomRemoteViewportDevClient
  DevRemoteViewportClient.cpp
  HeadlessCommandProtocol.cpp
  HeadlessOverlayModule.cpp
  HostModules.cpp
  HeadlessSessionHost.cpp
  HeadlessSessionModule.cpp
  WebRtcSession.cpp
)

axiom_apply_performance_options(AxiomRemoteViewportDevClient)

target_include_directories(AxiomHeadless PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_include_directories(AxiomRemoteViewportDevClient PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_link_libraries(AxiomHeadless PRIVATE
  AxiomCore
  AxiomScene
  AxiomAssets
  AxiomRenderer
  AxiomNet
)
if(TARGET AxiomPhysics)
  target_link_libraries(AxiomHeadless PRIVATE AxiomPhysics)
endif()
if(TARGET AxiomScripting)
  target_link_libraries(AxiomHeadless PRIVATE AxiomScripting)
endif()
target_link_libraries(AxiomRemoteViewportDevClient PRIVATE
  AxiomCore
  AxiomScene
  AxiomAssets
  AxiomRenderer
  AxiomNet
)
if(TARGET AxiomPhysics)
  target_link_libraries(AxiomRemoteViewportDevClient PRIVATE AxiomPhysics)
endif()
if(TARGET AxiomScripting)
  target_link_libraries(AxiomRemoteViewportDevClient PRIVATE AxiomScripting)
endif()

add_executable(AxiomRemoteViewportServer
  AxiomRemoteViewportServer.cpp
  HeadlessCommandProtocol.cpp
  HeadlessOverlayModule.cpp
  HostModules.cpp
  HeadlessSessionHost.cpp
  HeadlessSessionModule.cpp
  WebRtcSession.cpp
)

axiom_apply_performance_options(AxiomRemoteViewportServer)

add_executable(AxiomPackagedRuntime
  AxiomPackagedRuntime.cpp
  HeadlessOverlayModule.cpp
  HostModules.cpp
  PackagedRuntimeHost.cpp
  HeadlessSessionModule.cpp
)

axiom_apply_performance_options(AxiomPackagedRuntime)

target_include_directories(AxiomRemoteViewportServer PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/deps"
)

target_link_libraries(AxiomRemoteViewportServer PRIVATE
  AxiomCore
  AxiomScene
  AxiomAssets
  AxiomRenderer
  AxiomNet
  WraithNetworking
)
if(TARGET AxiomPhysics)
  target_link_libraries(AxiomRemoteViewportServer PRIVATE AxiomPhysics)
endif()
if(TARGET AxiomScripting)
  target_link_libraries(AxiomRemoteViewportServer PRIVATE AxiomScripting)
endif()
target_link_libraries(AxiomPackagedRuntime PRIVATE
  AxiomCore
  AxiomScene
  AxiomAssets
  AxiomRenderer
  AxiomNet
)
if(TARGET AxiomPhysics)
  target_link_libraries(AxiomPackagedRuntime PRIVATE AxiomPhysics)
endif()
if(TARGET AxiomScripting)
  target_link_libraries(AxiomPackagedRuntime PRIVATE AxiomScripting)
endif()

foreach(AXIOM_HEADLESS_TARGET IN ITEMS
    AxiomHeadless
    AxiomRemoteViewportDevClient
    AxiomRemoteViewportServer
    AxiomPackagedRuntime)
  target_compile_definitions(${AXIOM_HEADLESS_TARGET} PRIVATE
    AXIOM_WITH_PHYSICS=$<IF:$<BOOL:${AXIOM_ENABLE_PHYSICS}>,1,0>
    AXIOM_WITH_SCRIPTING=$<IF:$<BOOL:${AXIOM_ENABLE_SCRIPTING}>,1,0>
  )
endforeach()

set_target_properties(AxiomPackagedRuntime PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Headless"
)

target_compile_definitions(AxiomScene PUBLIC
  AXIOM_PACKAGED_RUNTIME_BINARY_PATH="$<TARGET_FILE:AxiomPackagedRuntime>"
)
