#include "Renderer/Vulkan/VulkanRendererBackend.h"
#include "Renderer/Vulkan/VulkanDescriptors.h"
#include "Renderer/Vulkan/VulkanPipeline.h"

#include <volk.h>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vma/vk_mem_alloc.h>

#include <cassert>
#include <chrono>
#include <cmath>
#include <thread>
#include <utility>

#include "Core/Log.h"
#include "Core/Window.h"
#include "Renderer/Vulkan/VulkanImage.h"
#include "Renderer/Vulkan/VulkanInitializers.h"
#include "Renderer/Vulkan/VulkanTypes.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

Axiom::VulkanRendererBackend *g_LoadedEngine = nullptr;

Axiom::VulkanRendererBackend &Axiom::VulkanRendererBackend::Get() { return *g_LoadedEngine; }

void Axiom::VulkanRendererBackend::Init(const RendererCreateInfo &CreateInfo) {
  assert(CreateInfo.TargetWindow != nullptr);
  assert(g_LoadedEngine == nullptr);
  g_LoadedEngine = this;

  m_Window = CreateInfo.TargetWindow->GetNativeHandle();
  m_WindowExtent = {CreateInfo.Width, CreateInfo.Height};

  m_Context.Init(m_Window);
  m_Device.Init(m_Context);
  InitSwapchain();
  m_CommandContext.Init(m_Device.Device, m_Device.GraphicsQueueFamily);
  InitDescriptors();
  InitPipelines();
  InitImGui();

  // Everything went fine
  m_IsInitialized = m_Window != nullptr;

  A_CORE_INFO("Vulkan Engine set up was successful: {0}",
              m_IsInitialized ? "True" : "False");
}

void Axiom::VulkanRendererBackend::InitSwapchain() {
  m_Swapchain.Init(m_Context, m_Device, m_WindowExtent.width,
                   m_WindowExtent.height);

  // Draw Image Size will match the window
  VkExtent3D DrawImageExtent = {m_WindowExtent.width, m_WindowExtent.height, 1};

  // Hardcoding the draw format to 64 bits per pixel
  m_DrawImage.ImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
  m_DrawImage.ImageExtent = DrawImageExtent;

  VkImageUsageFlags DrawImageUsages{};
  DrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  DrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  DrawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
  DrawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageCreateInfo RImgInfo = VkInit::ImageCreateInfo(
      m_DrawImage.ImageFormat, DrawImageUsages, m_DrawImage.ImageExtent);

  // For the draw image, we want to allocate it from GPU local memory
  VmaAllocationCreateInfo RImgAllocInfo = {};
  RImgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  RImgAllocInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Allocate and create the image
  vmaCreateImage(m_Device.Allocator, &RImgInfo, &RImgAllocInfo, &m_DrawImage.Image,
                 &m_DrawImage.Allocation, VK_NULL_HANDLE);

  // Build an image-view for the draw image to use for rendering
  VkImageViewCreateInfo RViewInfo = VkInit::ImageViewCreateInfo(
      m_DrawImage.ImageFormat, m_DrawImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device.Device, &RViewInfo, VK_NULL_HANDLE,
                             &m_DrawImage.ImageView));

  // Add to deletion queue
  m_MainDeletionQueue.PushFunction([&]() {
    vkDestroyImageView(m_Device.Device, m_DrawImage.ImageView, VK_NULL_HANDLE);
    vmaDestroyImage(m_Device.Allocator, m_DrawImage.Image, m_DrawImage.Allocation);
  });
}

void Axiom::VulkanRendererBackend::Shutdown() {
  A_CORE_INFO("Running Vulkan renderer cleanup...");

  if (m_IsInitialized) {
    vkDeviceWaitIdle(m_Device.Device);

    m_CommandContext.Shutdown(m_Device.Device);
    m_MainDeletionQueue.Flush();
    m_Swapchain.Shutdown(m_Device);
    m_Device.Shutdown();
    m_Context.Shutdown();

    m_IsInitialized = false;
  }

  g_LoadedEngine = nullptr;
}

void Axiom::VulkanRendererBackend::InitDescriptors() {
  // Create a descriptor pool that will hold 10 sets with 1 image each
  std::vector<DescriptorAllocator::PoolSizeRatio> Sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};

  m_GlobalDescriptorAllocator.InitPool(m_Device.Device, 10, Sizes);

  // Make the descriptor set layout for our compute draw
  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_DrawImageDescriptorLayout =
        Builder.Build(m_Device.Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  // Allocate a descriptor set for our draw image
  m_DrawImageDescriptors = m_GlobalDescriptorAllocator.Allocate(
      m_Device.Device, m_DrawImageDescriptorLayout);

  VkDescriptorImageInfo ImageInfo{};
  ImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  ImageInfo.imageView = m_DrawImage.ImageView;

  VkWriteDescriptorSet DrawImageWrite = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = VK_NULL_HANDLE};
  DrawImageWrite.dstBinding = 0;
  DrawImageWrite.dstSet = m_DrawImageDescriptors;
  DrawImageWrite.descriptorCount = 1;
  DrawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  DrawImageWrite.pImageInfo = &ImageInfo;

  vkUpdateDescriptorSets(m_Device.Device, 1, &DrawImageWrite, 0, VK_NULL_HANDLE);

  // Make sure both the descriptor allocator and the new layout get cleaned up
  // properly
  m_MainDeletionQueue.PushFunction([&]() {
    m_GlobalDescriptorAllocator.DestroyPool(m_Device.Device);
    vkDestroyDescriptorSetLayout(m_Device.Device, m_DrawImageDescriptorLayout,
                                 VK_NULL_HANDLE);
  });
}

void Axiom::VulkanRendererBackend::InitPipelines() { InitBackgroundPipelines(); }

void Axiom::VulkanRendererBackend::InitBackgroundPipelines() {
  VkPipelineLayoutCreateInfo ComputeLayout = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = VK_NULL_HANDLE};
  ComputeLayout.pSetLayouts = &m_DrawImageDescriptorLayout;
  ComputeLayout.setLayoutCount = 1;

  VkPushConstantRange PushConstant{};
  PushConstant.offset = 0;
  PushConstant.size = sizeof(ComputePushConstants);
  PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  ComputeLayout.pPushConstantRanges = &PushConstant;
  ComputeLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &ComputeLayout, VK_NULL_HANDLE,
                                  &m_GradientPipelineLayout));

  VkShaderModule ComputeDrawShader;
  if (!VkUtil::LoadShaderModule("../../Content/Shaders/gradient_color.comp.spv",
                                m_Device.Device, &ComputeDrawShader)) {
    A_ERROR("Error when building the compute shader");
  }

  VkPipelineShaderStageCreateInfo StageInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE};
  StageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  StageInfo.module = ComputeDrawShader;
  StageInfo.pName = "main";

  VkComputePipelineCreateInfo ComputePipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE};
  ComputePipelineCreateInfo.layout = m_GradientPipelineLayout;
  ComputePipelineCreateInfo.stage = StageInfo;

  VK_CHECK(vkCreateComputePipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                    &ComputePipelineCreateInfo, VK_NULL_HANDLE,
                                    &m_GradientPipeline));

  vkDestroyShaderModule(m_Device.Device, ComputeDrawShader, VK_NULL_HANDLE);

  m_MainDeletionQueue.PushFunction([&]() {
    vkDestroyPipelineLayout(m_Device.Device, m_GradientPipelineLayout, VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_GradientPipeline, VK_NULL_HANDLE);
  });
}

void Axiom::VulkanRendererBackend::DrawImGui(VkCommandBuffer Command,
                             VkImageView TargetImageView) {
  VkRenderingAttachmentInfo ColorAttachment =
      VkInit::AttachmentInfo(TargetImageView, VK_NULL_HANDLE,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo RenderingInfo = VkInit::RenderingInfo(
      m_Swapchain.Extent, &ColorAttachment, VK_NULL_HANDLE);

  vkCmdBeginRendering(Command, &RenderingInfo);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Command);

  vkCmdEndRendering(Command);
}

void Axiom::VulkanRendererBackend::DrawBackground(VkCommandBuffer CommandBuffer) {
  // Vind the gradient drawing compute pipeline
  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    m_GradientPipeline);

  // Bind the descriptor set containing the draw image for the compute pipeline
  vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_GradientPipelineLayout, 0, 1,
                          &m_DrawImageDescriptors, 0, VK_NULL_HANDLE);

  ComputePushConstants PC;
  PC.data1 = glm::vec4(1, 0, 0, 1);
  PC.data2 = glm::vec4(0, 0, 1, 1);

  vkCmdPushConstants(CommandBuffer, m_GradientPipelineLayout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0,
                     sizeof(ComputePushConstants), &PC);

  // Execute the compute pipeline dispatch. We are using 16x16 workgroup size so
  // we need to factor that in.
  vkCmdDispatch(CommandBuffer, std::ceil(m_DrawExtent.width / 16.0),
                std::ceil(m_DrawExtent.height / 16.0), 1);
}

void Axiom::VulkanRendererBackend::InitImGui() {
  // Create descriptor pool for ImGui
  // The size of the pool is very oversize, btu it's copied from the ImGui demo
  // code.
  VkDescriptorPoolSize PoolSizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo PoolInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT};
  PoolInfo.maxSets = 1000;
  PoolInfo.poolSizeCount = static_cast<uint32_t>(std::size(PoolSizes));
  PoolInfo.pPoolSizes = PoolSizes;

  VkDescriptorPool ImGuiPool;
  VK_CHECK(
      vkCreateDescriptorPool(m_Device.Device, &PoolInfo, VK_NULL_HANDLE, &ImGuiPool));

  // Initialize ImGui Library
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForVulkan(m_Window, true);

  bool LoadedImGuiVulkanFunctions = ImGui_ImplVulkan_LoadFunctions(
      VK_API_VERSION_1_3,
      [](const char *FunctionName, void *UserData) {
        return vkGetInstanceProcAddr(static_cast<VkInstance>(UserData),
                                     FunctionName);
      },
      m_Context.Instance);
  assert(LoadedImGuiVulkanFunctions);

  ImGui_ImplVulkan_InitInfo InitInfo = {};
  InitInfo.ApiVersion = VK_API_VERSION_1_3;
  InitInfo.Instance = m_Context.Instance;
  InitInfo.PhysicalDevice = m_Device.PhysicalDevice;
  InitInfo.Device = m_Device.Device;
  InitInfo.Queue = m_Device.GraphicsQueue;
  InitInfo.DescriptorPool = ImGuiPool;
  InitInfo.MinImageCount = 3;
  InitInfo.ImageCount = 3;
  InitInfo.UseDynamicRendering = true;

  // Dynamic rendering params for ImGui to use
  ImGui_ImplVulkan_PipelineInfo PipelineInfo = {};
  PipelineInfo.PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  PipelineInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  PipelineInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats =
      &m_Swapchain.ImageFormat;

  PipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  InitInfo.QueueFamily = m_Device.GraphicsQueueFamily;
  InitInfo.PipelineInfoMain = PipelineInfo;
  ImGui_ImplVulkan_Init(&InitInfo);

  m_MainDeletionQueue.PushFunction([this, ImGuiPool]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(m_Device.Device, ImGuiPool, VK_NULL_HANDLE);
  });
}

void Axiom::VulkanRendererBackend::Draw() {
  // Wait until the GPU has finished rendering the last frame
  // Timeout of 1 second
  VK_CHECK(vkWaitForFences(m_Device.Device, 1, &GetCurrentFrame().RenderFence, VK_TRUE,
                           1000000000));
  VK_CHECK(vkResetFences(m_Device.Device, 1, &GetCurrentFrame().RenderFence));

  GetCurrentFrame().DeletionQueue.Flush();

  // Request image from the swapchain
  uint32_t SwapchainImageIndex;
  VK_CHECK(vkAcquireNextImageKHR(m_Device.Device, m_Swapchain.Swapchain, 1000000000,
                                 GetCurrentFrame().SwapchainSemaphore,
                                 VK_NULL_HANDLE, &SwapchainImageIndex));

  VkCommandBuffer CommandBuffer = GetCurrentFrame().MainCommandBuffer;

  // Now that we are sure the commands are finished executing (the fences), we
  // can safely reset the command buffer to begin recording again.
  VK_CHECK(vkResetCommandBuffer(CommandBuffer, 0));

  // Begin the command buffer recording.
  // We will use this command buffer exactly once, so we need to let Vulkan know
  // that.
  VkCommandBufferBeginInfo CommandBufferBeginInfo =
      VkInit::CommandBufferBeginInfo(
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  m_DrawExtent.width = m_DrawImage.ImageExtent.width;
  m_DrawExtent.height = m_DrawImage.ImageExtent.height;

  // Start recording
  VK_CHECK(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo));

  // Transition our main draw image into general layout so we can write into it
  // We will overwrite it all so we don't care about what was in the older
  // layout
  VkUtil::TransitionImage(CommandBuffer, m_DrawImage.Image,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  // Clear color
  DrawBackground(CommandBuffer);

  // Transition the draw image and the swapchain image into their correct
  // transfer layouts
  VkUtil::TransitionImage(CommandBuffer, m_DrawImage.Image,
                          VK_IMAGE_LAYOUT_GENERAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  VkUtil::TransitionImage(CommandBuffer, m_Swapchain.Images[SwapchainImageIndex],
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // Execute a copy fromt eh draw image into the swapchain
  VkUtil::CopyImageToImage(CommandBuffer, m_DrawImage.Image,
                           m_Swapchain.Images[SwapchainImageIndex], m_DrawExtent,
                           m_Swapchain.Extent);

  // Set swapchain image layout to Attachment Optimal so we can draw it
  VkUtil::TransitionImage(CommandBuffer, m_Swapchain.Images[SwapchainImageIndex],
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // Draw ImGui into the swapchain image
  DrawImGui(CommandBuffer, m_Swapchain.ImageViews[SwapchainImageIndex]);

  // Set swapchain image layot to Present so we can draw it
  VkUtil::TransitionImage(CommandBuffer, m_Swapchain.Images[SwapchainImageIndex],
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // Finalize the command buffer
  VK_CHECK(vkEndCommandBuffer(CommandBuffer));

  // Prepare the submission to the queue
  // Use the swapchain semaphore to signal that the swapchain is ready
  // Use the render semaphore to signal everything is ready to be rendered
  VkCommandBufferSubmitInfo CommandBufferSubmitInfo =
      VkInit::CommandBufferSubmitInfo(CommandBuffer);

  VkSemaphoreSubmitInfo SwapchainSemaphoreSubmitInfo =
      VkInit::SemaphoreSubmitInfo(
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
          GetCurrentFrame().SwapchainSemaphore);
  VkSemaphoreSubmitInfo RenderSemaphoreSubmitInfo = VkInit::SemaphoreSubmitInfo(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().RenderSemaphore);

  VkSubmitInfo2 Submit = // We basically want to wait for the swapchain
                         // semaphore to signal because that's what happens with
                         // the vkAcquireNextImageKHR call above, then signal
                         // the render semaphore when this is ready
      VkInit::SubmitInfo(&CommandBufferSubmitInfo, &RenderSemaphoreSubmitInfo,
                         &SwapchainSemaphoreSubmitInfo);

  // Submit the command buffer to the queue and execute it
  // RenderFence will block (first line in this function) until the graphics
  // commands finish executing
  VK_CHECK(vkQueueSubmit2(m_Device.GraphicsQueue, 1, &Submit,
                          GetCurrentFrame().RenderFence));

  // This will put the image we just rendered into the visible window
  VkPresentInfoKHR PresentInfo = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                  .pNext = VK_NULL_HANDLE};
  PresentInfo.pSwapchains = &m_Swapchain.Swapchain;
  PresentInfo.swapchainCount = 1;

  // We want to wait on the RenderSemaphore for that as it's necessary that
  // drawing commands have finished before the image is displayed (to prevent
  // tearing)
  PresentInfo.pWaitSemaphores = &GetCurrentFrame().RenderSemaphore;
  PresentInfo.waitSemaphoreCount = 1;

  PresentInfo.pImageIndices = &SwapchainImageIndex;

  VK_CHECK(vkQueuePresentKHR(m_Device.GraphicsQueue, &PresentInfo));

  // End of frame
  m_FrameNumber++;
}

void Axiom::VulkanRendererBackend::ImmediateSubmit(
    std::function<void(VkCommandBuffer Command)> &&Function) {
  m_CommandContext.ImmediateSubmit(m_Device.Device, m_Device.GraphicsQueue,
                                   std::move(Function));
}

void Axiom::VulkanRendererBackend::BeginFrame() {
  m_StopRendering = glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED);
  m_RenderFallbackBackground = false;
  if (m_StopRendering) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return;
  }

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void Axiom::VulkanRendererBackend::RenderFallbackBackground(RenderScene &Scene) {
  (void)Scene;
  m_RenderFallbackBackground = true;
}

void Axiom::VulkanRendererBackend::RenderImGui() {
  if (m_StopRendering) {
    return;
  }

  ImGui::ShowDemoWindow();
  ImGui::Render();
}

void Axiom::VulkanRendererBackend::EndFrame() {
  if (m_StopRendering) {
    return;
  }

  Draw();
}



