add_library(AxiomHAL STATIC
  DynamicLibrary.cpp
  FileWatcher.cpp
  Platform.cpp
  PlatformMedia.cpp
  Socket.cpp
  SvgRasterizer.cpp
)

if(APPLE)
  target_sources(AxiomHAL PRIVATE
    MacOS/MacOSFileWatcher.cpp
    MacOS/MacOSSvgRasterizer.cpp
    MacOS/MacOSVideoToolboxH264Encoder.mm
    MacOS/MacOSWebRtcSession.mm
  )
endif()

target_include_directories(AxiomHAL PUBLIC
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/Axiom"
  "${CMAKE_SOURCE_DIR}/Headless"
  "${CMAKE_SOURCE_DIR}/ThirdParty/fastgltf/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/glm"
  "${CMAKE_SOURCE_DIR}/ThirdParty/spdlog/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/stb_image"
  "${rapidjson_SOURCE_DIR}/include"
  "$<$<BOOL:${AXIOM_ENABLE_PHYSICS}>:${AXIOM_JOLT_SOURCE_DIR}>"
)

target_include_directories(AxiomHAL PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/vkbootstrap"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vma"
  "${CMAKE_SOURCE_DIR}/ThirdParty/volk"
  "${Vulkan_INCLUDE_DIRS}"
)

target_compile_definitions(AxiomHAL PUBLIC
  AXIOM_WEBRTC_LINKED=${AXIOM_WEBRTC_LINKED}
)

if(AXIOM_ENABLE_WEBRTC)
  target_compile_definitions(AxiomHAL PUBLIC AXIOM_ENABLE_WEBRTC=1)
else()
  target_compile_definitions(AxiomHAL PUBLIC AXIOM_ENABLE_WEBRTC=0)
endif()

if(AXIOM_SCRIPTING_WATCH AND APPLE)
  target_compile_definitions(AxiomHAL PUBLIC AXIOM_SCRIPTING_WATCH=1)
else()
  target_compile_definitions(AxiomHAL PUBLIC AXIOM_SCRIPTING_WATCH=0)
endif()

if(AXIOM_VULKAN_LOADER_PATH)
  string(REPLACE "\\" "\\\\" AXIOM_VULKAN_LOADER_PATH_ESCAPED
         "${AXIOM_VULKAN_LOADER_PATH}")
  target_compile_definitions(AxiomHAL PUBLIC
    AXIOM_VULKAN_LOADER_PATH="${AXIOM_VULKAN_LOADER_PATH_ESCAPED}"
  )
endif()

if(AXIOM_WEBRTC_COMPILE_OPTIONS)
  target_compile_options(AxiomHAL PUBLIC ${AXIOM_WEBRTC_COMPILE_OPTIONS})
endif()

if(AXIOM_WEBRTC_LINK_OPTIONS)
  target_link_options(AxiomHAL PUBLIC ${AXIOM_WEBRTC_LINK_OPTIONS})
endif()

if(AXIOM_WEBRTC_LIBRARIES)
  target_link_libraries(AxiomHAL PUBLIC ${AXIOM_WEBRTC_LIBRARIES})
endif()

if(AXIOM_ENABLE_WEBRTC AND TARGET AxiomWebRTC)
  target_link_libraries(AxiomHAL PUBLIC AxiomWebRTC)
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(AxiomHAL PUBLIC ${CMAKE_DL_LIBS})
endif()

if(APPLE)
  target_link_libraries(AxiomHAL PUBLIC
    "-framework CoreFoundation"
    "-framework CoreGraphics"
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework Foundation"
    "-framework VideoToolbox"
  )
endif()
