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

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(AxiomCore PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g -march=native>
    $<$<CONFIG:MinSizeRel>:-Os>
  )
endif()
