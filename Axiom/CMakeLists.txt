set(ENGINE_SOURCES
  Assets/MeshAsset.cpp
  Core/Application.cpp
  Core/Layer.cpp
  Core/LayerStack.cpp
  Core/Window.cpp
  Core/Log.cpp
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
  Renderer/Vulkan/VulkanInitializers.cpp
  Renderer/Vulkan/VulkanPipeline.cpp
  Renderer/Vulkan/VulkanRendererBackend.cpp
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
  AXIOM_VULKAN_LOADER_LIBRARY="${Vulkan_LIBRARIES}"
)

target_compile_options(AxiomCore PRIVATE -O3 -march=native)
