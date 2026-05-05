set(ENGINE_SOURCES
  Assets/MeshAsset.cpp
  Core/Application.cpp
  Core/GlfwEditorInputSource.cpp
  Core/GlfwWindow.cpp
  Core/HeadlessWindow.cpp
  Core/Layer.cpp
  Core/LayerStack.cpp
  Core/Window.cpp
  Core/WindowInputPlatform.cpp
  Core/Log.cpp
  Core/VulkanLoader.cpp
  Remote/AxiomSessionEndpoint.cpp
  Session/BufferedEditorInputSource.cpp
  Session/EditorMessageBus.cpp
  Session/EditorSession.cpp
  Session/StartupScene.cpp
  Renderer/Camera.cpp
  Renderer/ForwardRenderer.cpp
  Renderer/RenderCommand.cpp
  Renderer/Renderer.cpp
  Renderer/RenderScene.cpp
  Renderer/VideoEncoderFactory.cpp
  Renderer/Vulkan/VulkanBuffer.cpp
  Renderer/Vulkan/VulkanCommandContext.cpp
  Renderer/Vulkan/VulkanContext.cpp
  Renderer/Vulkan/VulkanDescriptors.cpp
  Renderer/Vulkan/VulkanDevice.cpp
  Renderer/Vulkan/VulkanImage.cpp
  Renderer/Vulkan/VulkanImGuiRenderer.cpp
  Renderer/Vulkan/VulkanInitializers.cpp
  Renderer/Vulkan/VulkanMaterialResources.cpp
  Renderer/Vulkan/VulkanMesh.cpp
  Renderer/Vulkan/VulkanOcclusionCulling.cpp
  Renderer/Vulkan/VulkanPipeline.cpp
  Renderer/Vulkan/VulkanRendererBackend.cpp
  Renderer/Vulkan/VulkanSceneRenderer.cpp
  Renderer/Vulkan/VulkanSwapchain.cpp
)

if(APPLE)
  list(APPEND ENGINE_SOURCES
    Renderer/MacOSVideoToolboxH264Encoder.mm
  )
endif()

set(THIRDPARTY_SOURCES
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui.h"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui.cpp"

  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_demo.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_draw.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_widgets.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_tables.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_impl_vulkan.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_impl_glfw.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/volk/volk.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vkbootstrap/VkBootstrap.cpp"
)

add_library(AxiomCore STATIC ${ENGINE_SOURCES} ${THIRDPARTY_SOURCES})

add_dependencies(AxiomCore Shaders)

set(AXIOM_WEBRTC_LINKED 0)

if(AXIOM_ENABLE_WEBRTC)
  if(NOT APPLE)
    message(FATAL_ERROR
      "AXIOM_ENABLE_WEBRTC is currently supported only on macOS.")
  endif()

  if(AXIOM_WEBRTC_FRAMEWORK_PATH)
    set(AXIOM_WEBRTC_FRAMEWORK_HEADERS
        "${AXIOM_WEBRTC_FRAMEWORK_PATH}/Headers")
    set(AXIOM_WEBRTC_FRAMEWORK_BINARY
        "${AXIOM_WEBRTC_FRAMEWORK_PATH}/WebRTC")
    if(NOT EXISTS "${AXIOM_WEBRTC_FRAMEWORK_HEADERS}")
      message(FATAL_ERROR
        "AXIOM_WEBRTC_FRAMEWORK_PATH does not contain a Headers directory: ${AXIOM_WEBRTC_FRAMEWORK_PATH}")
    endif()
    if(NOT EXISTS "${AXIOM_WEBRTC_FRAMEWORK_BINARY}")
      message(FATAL_ERROR
        "AXIOM_WEBRTC_FRAMEWORK_PATH does not contain the framework binary: ${AXIOM_WEBRTC_FRAMEWORK_BINARY}")
    endif()

    add_library(AxiomWebRTC UNKNOWN IMPORTED GLOBAL)
    set_target_properties(AxiomWebRTC PROPERTIES
      IMPORTED_LOCATION "${AXIOM_WEBRTC_FRAMEWORK_BINARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${AXIOM_WEBRTC_FRAMEWORK_HEADERS}"
    )
    set(AXIOM_WEBRTC_LINKED 1)
    message(STATUS "WebRTC framework: ${AXIOM_WEBRTC_FRAMEWORK_PATH}")
  elseif(AXIOM_WEBRTC_LIBRARY_PATH OR AXIOM_WEBRTC_INCLUDE_DIR)
    if(NOT AXIOM_WEBRTC_LIBRARY_PATH OR NOT AXIOM_WEBRTC_INCLUDE_DIR)
      message(FATAL_ERROR
        "When not using AXIOM_WEBRTC_FRAMEWORK_PATH, both AXIOM_WEBRTC_LIBRARY_PATH and AXIOM_WEBRTC_INCLUDE_DIR must be set.")
    endif()
    if(NOT EXISTS "${AXIOM_WEBRTC_LIBRARY_PATH}")
      message(FATAL_ERROR
        "AXIOM_WEBRTC_LIBRARY_PATH does not exist: ${AXIOM_WEBRTC_LIBRARY_PATH}")
    endif()
    if(NOT EXISTS "${AXIOM_WEBRTC_INCLUDE_DIR}")
      message(FATAL_ERROR
        "AXIOM_WEBRTC_INCLUDE_DIR does not exist: ${AXIOM_WEBRTC_INCLUDE_DIR}")
    endif()

    add_library(AxiomWebRTC UNKNOWN IMPORTED GLOBAL)
    set_target_properties(AxiomWebRTC PROPERTIES
      IMPORTED_LOCATION "${AXIOM_WEBRTC_LIBRARY_PATH}"
      INTERFACE_INCLUDE_DIRECTORIES "${AXIOM_WEBRTC_INCLUDE_DIR}"
    )
    set(AXIOM_WEBRTC_LINKED 1)
    message(STATUS "WebRTC library: ${AXIOM_WEBRTC_LIBRARY_PATH}")
    message(STATUS "WebRTC includes: ${AXIOM_WEBRTC_INCLUDE_DIR}")
  else()
    message(FATAL_ERROR
      "AXIOM_ENABLE_WEBRTC=ON requires either AXIOM_WEBRTC_FRAMEWORK_PATH or both AXIOM_WEBRTC_LIBRARY_PATH and AXIOM_WEBRTC_INCLUDE_DIR.")
  endif()
endif()

set(AXIOM_VULKAN_LOADER_PATH "" CACHE STRING
    "Optional explicit Vulkan loader path used when platform-default discovery fails")

find_package(Vulkan REQUIRED)
message(STATUS "Vulkan_FOUND: ${Vulkan_FOUND}")
message(STATUS "Vulkan_INCLUDE_DIRS: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan_LIBRARIES: ${Vulkan_LIBRARIES}")
message(STATUS "Vulkan_VERSION: ${Vulkan_VERSION}")

target_include_directories(AxiomCore PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui"
  "${CMAKE_SOURCE_DIR}/ThirdParty/fastgltf/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/glm"
  "${CMAKE_SOURCE_DIR}/ThirdParty/stb_image"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vkbootstrap"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vma"
  "${CMAKE_SOURCE_DIR}/ThirdParty/volk"
  "${CMAKE_SOURCE_DIR}/ThirdParty/spdlog/include"
  "${Vulkan_INCLUDE_DIRS}"
)

target_link_libraries(AxiomCore PUBLIC 
  glfw Vulkan::Vulkan fastgltf glm
  spdlog
)

target_compile_definitions(AxiomCore PUBLIC
  VK_NO_PROTOTYPES
  AXIOM_CONTENT_DIR="${CMAKE_SOURCE_DIR}/Content"
)

if(AXIOM_ENABLE_WEBRTC)
  target_compile_definitions(AxiomCore PUBLIC AXIOM_ENABLE_WEBRTC=1)
else()
  target_compile_definitions(AxiomCore PUBLIC AXIOM_ENABLE_WEBRTC=0)
endif()

target_compile_definitions(AxiomCore PUBLIC
  AXIOM_WEBRTC_LINKED=${AXIOM_WEBRTC_LINKED}
)

if(AXIOM_VULKAN_LOADER_PATH)
  string(REPLACE "\\" "\\\\" AXIOM_VULKAN_LOADER_PATH_ESCAPED
         "${AXIOM_VULKAN_LOADER_PATH}")
  target_compile_definitions(AxiomCore PUBLIC
    AXIOM_VULKAN_LOADER_PATH="${AXIOM_VULKAN_LOADER_PATH_ESCAPED}"
  )
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(AxiomCore PUBLIC ${CMAKE_DL_LIBS})
endif()

if(APPLE)
  target_link_libraries(AxiomCore PUBLIC
    "-framework Foundation"
    "-framework CoreFoundation"
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework VideoToolbox"
  )
endif()

if(AXIOM_ENABLE_WEBRTC AND TARGET AxiomWebRTC)
  target_link_libraries(AxiomCore PUBLIC AxiomWebRTC)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(AxiomCore PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g -march=native>
    $<$<CONFIG:MinSizeRel>:-Os>
  )
endif()
