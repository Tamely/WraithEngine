#include "Renderer/Vulkan/VulkanRendererBackend.h"

#include "Assets/SvgTexture.h"
#include "Renderer/Camera.h"
#include "Renderer/RenderScene.h"
#include "Renderer/Vulkan/VulkanBuffer.h"
#include "Renderer/Vulkan/VulkanDescriptors.h"
#include "Renderer/Vulkan/VulkanImage.h"
#include "Renderer/Vulkan/VulkanInitializers.h"
#include "Renderer/Vulkan/VulkanMesh.h"
#include "Renderer/Vulkan/VulkanPipeline.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>

#include <volk.h>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vk_mem_alloc.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <string>
#include <thread>
#include <utility>

#include "Core/Log.h"

Axiom::VulkanRendererBackend *g_LoadedEngine = nullptr;

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

namespace Axiom {
namespace {
void TransitionImageRange(VkCommandBuffer CommandBuffer, VkImage Image,
                          VkImageLayout OldLayout, VkImageLayout NewLayout,
                          VkImageAspectFlags AspectMask, uint32_t BaseMipLevel,
                          uint32_t LevelCount) {
  VkImageMemoryBarrier2 ImageBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = VK_NULL_HANDLE,
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .dstAccessMask =
          VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
      .oldLayout = OldLayout,
      .newLayout = NewLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = Image,
      .subresourceRange =
          {.aspectMask = AspectMask,
           .baseMipLevel = BaseMipLevel,
           .levelCount = LevelCount,
           .baseArrayLayer = 0,
           .layerCount = 1}};
  VkDependencyInfo DependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = VK_NULL_HANDLE,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &ImageBarrier};
  vkCmdPipelineBarrier2(CommandBuffer, &DependencyInfo);
}
} // namespace

VulkanRendererBackend &VulkanRendererBackend::Get() { return *g_LoadedEngine; }

VulkanRendererBackend *VulkanRendererBackend::TryGet() { return g_LoadedEngine; }

void VulkanRendererBackend::Init(const RendererCreateInfo &CreateInfo) {
  assert(CreateInfo.TargetSurface != nullptr);
  assert(g_LoadedEngine == nullptr);
  g_LoadedEngine = this;

  m_Surface = CreateInfo.TargetSurface;
  m_FrameOutput = CreateInfo.FrameOutput;
  m_HasPresentationSurface = m_Surface->SupportsPresentation();
  m_EnableImGui = m_HasPresentationSurface;
  m_Window = m_HasPresentationSurface
                 ? static_cast<GLFWwindow *>(m_Surface->GetNativeWindowHandle())
                 : nullptr;
  m_WindowExtent = {CreateInfo.Width, CreateInfo.Height};

  m_Context.Init(m_Surface->GetNativeWindowHandle(), m_HasPresentationSurface);
  m_Device.Init(m_Context);

  VkPhysicalDeviceProperties DeviceProperties{};
  vkGetPhysicalDeviceProperties(m_Device.PhysicalDevice, &DeviceProperties);
  m_TimestampPeriod = DeviceProperties.limits.timestampPeriod;

  m_CommandContext.Init(m_Device.Device, m_Device.GraphicsQueueFamily);
  m_MaterialResources.Init({.Allocator = m_Device.Allocator,
                            .Device = m_Device.Device,
                            .GraphicsQueue = m_Device.GraphicsQueue,
                            .DeletionQueue = &m_MainDeletionQueue,
                            .ImmediateSubmit =
                                [this](std::function<void(VkCommandBuffer cmd)> &&
                                           Function) {
                                  ImmediateSubmit(std::move(Function));
                                }});
  m_OcclusionCulling.Init(m_Device.Device, m_Device.Allocator);

  InitSwapchain();
  InitHzbResources();
  InitViewportReadbackBuffers();
  InitDescriptors();
  InitTextureResources();
  InitPipelines();
  InitMeshFrameResources();
  if (m_EnableImGui) {
    m_ImGuiRenderer.Init({.WindowHandle = m_Window,
                          .Instance = m_Context.Instance,
                          .PhysicalDevice = m_Device.PhysicalDevice,
                          .Device = m_Device.Device,
                          .Queue = m_Device.GraphicsQueue,
                          .QueueFamily = m_Device.GraphicsQueueFamily,
                          .SwapchainImageFormat = m_Swapchain.ImageFormat,
                          .DeletionQueue = &m_MainDeletionQueue});
  }

  m_IsInitialized = true;

  A_CORE_INFO("Vulkan Engine set up was successful: {0}",
              m_IsInitialized ? "True" : "False");
}

void VulkanRendererBackend::InitSwapchain() {
  if (m_HasPresentationSurface) {
    m_Swapchain.Init(m_Context, m_Device, m_WindowExtent.width,
                     m_WindowExtent.height);
  } else {
    m_Swapchain.ImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    m_Swapchain.Extent = m_WindowExtent;
  }

  VkExtent3D DrawImageExtent = {m_WindowExtent.width, m_WindowExtent.height, 1};

  m_DrawImage.ImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
  m_DrawImage.ImageExtent = DrawImageExtent;

  VkImageUsageFlags DrawImageUsages{};
  DrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  DrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  DrawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
  DrawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageCreateInfo DrawInfo = VkInit::ImageCreateInfo(
      m_DrawImage.ImageFormat, DrawImageUsages, m_DrawImage.ImageExtent);

  VmaAllocationCreateInfo DrawAllocInfo{};
  DrawAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  DrawAllocInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VK_CHECK(vmaCreateImage(m_Device.Allocator, &DrawInfo, &DrawAllocInfo,
                          &m_DrawImage.Image, &m_DrawImage.Allocation,
                          VK_NULL_HANDLE));

  VkImageViewCreateInfo DrawViewInfo = VkInit::ImageViewCreateInfo(
      m_DrawImage.ImageFormat, m_DrawImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device.Device, &DrawViewInfo, VK_NULL_HANDLE,
                             &m_DrawImage.ImageView));

  m_DepthImage.ImageFormat = VK_FORMAT_R32_UINT;
  m_DepthImage.ImageExtent = DrawImageExtent;

  VkImageUsageFlags DepthUsages{};
  DepthUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  DepthUsages |= VK_IMAGE_USAGE_STORAGE_BIT;

  VkImageCreateInfo DepthInfo = VkInit::ImageCreateInfo(
      m_DepthImage.ImageFormat, DepthUsages, m_DepthImage.ImageExtent);
  VK_CHECK(vmaCreateImage(m_Device.Allocator, &DepthInfo, &DrawAllocInfo,
                          &m_DepthImage.Image, &m_DepthImage.Allocation,
                          VK_NULL_HANDLE));

  VkImageViewCreateInfo DepthViewInfo = VkInit::ImageViewCreateInfo(
      m_DepthImage.ImageFormat, m_DepthImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device.Device, &DepthViewInfo, VK_NULL_HANDLE,
                             &m_DepthImage.ImageView));

  m_RasterDepthImage.ImageFormat = VK_FORMAT_D32_SFLOAT;
  m_RasterDepthImage.ImageExtent = DrawImageExtent;

  VkImageUsageFlags RasterDepthUsages{};
  RasterDepthUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  RasterDepthUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

  VkImageCreateInfo RasterDepthInfo = VkInit::ImageCreateInfo(
      m_RasterDepthImage.ImageFormat, RasterDepthUsages,
      m_RasterDepthImage.ImageExtent);
  VK_CHECK(vmaCreateImage(m_Device.Allocator, &RasterDepthInfo, &DrawAllocInfo,
                          &m_RasterDepthImage.Image,
                          &m_RasterDepthImage.Allocation, VK_NULL_HANDLE));

  VkImageViewCreateInfo RasterDepthViewInfo = VkInit::ImageViewCreateInfo(
      m_RasterDepthImage.ImageFormat, m_RasterDepthImage.Image,
      VK_IMAGE_ASPECT_DEPTH_BIT);
  VK_CHECK(vkCreateImageView(m_Device.Device, &RasterDepthViewInfo,
                             VK_NULL_HANDLE, &m_RasterDepthImage.ImageView));

  m_MainDeletionQueue.PushFunction([this]() {
    for (auto &CaptureFrame : m_OffscreenCaptureFrames) {
      VkBufferUtil::DestroyBuffer(m_Device.Allocator, CaptureFrame.ReadbackBuffer);
      CaptureFrame = {};
    }
    vkDestroyImageView(m_Device.Device, m_RasterDepthImage.ImageView,
                       VK_NULL_HANDLE);
    vmaDestroyImage(m_Device.Allocator, m_RasterDepthImage.Image,
                    m_RasterDepthImage.Allocation);
    vkDestroyImageView(m_Device.Device, m_DepthImage.ImageView, VK_NULL_HANDLE);
    vmaDestroyImage(m_Device.Allocator, m_DepthImage.Image,
                    m_DepthImage.Allocation);
    vkDestroyImageView(m_Device.Device, m_DrawImage.ImageView, VK_NULL_HANDLE);
    vmaDestroyImage(m_Device.Allocator, m_DrawImage.Image,
                    m_DrawImage.Allocation);
  });
}

void VulkanRendererBackend::InitViewportReadbackBuffers() {
  const size_t BufferSize =
      static_cast<size_t>(m_DrawImage.ImageExtent.width) *
      static_cast<size_t>(m_DrawImage.ImageExtent.height) * sizeof(uint16_t) *
      4u;
  for (auto &CaptureFrame : m_OffscreenCaptureFrames) {
    CaptureFrame.ReadbackBuffer = VkBufferUtil::CreateBuffer(
        m_Device.Allocator, BufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_TO_CPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);
    CaptureFrame.HasPendingReadback = false;
    CaptureFrame.SubmittedFrameNumber = 0;
    CaptureFrame.SubmittedUser = {};
  }
}

void VulkanRendererBackend::InitHzbResources() {
  const VkExtent2D BaseExtent = {m_DrawImage.ImageExtent.width,
                                 m_DrawImage.ImageExtent.height};
  const uint32_t MipCount = ComputeHzbMipCount(BaseExtent);

  m_HzbMipImageViews.clear();
  m_HzbMipExtents.clear();
  m_HzbMipOffsets.clear();
  m_HzbReadbackBufferSize = 0;
  m_HzbImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  m_HzbImage.ImageFormat = VK_FORMAT_R32_SFLOAT;
  m_HzbImage.ImageExtent = {BaseExtent.width, BaseExtent.height, 1};

  VkImageCreateInfo HzbInfo =
      VkInit::ImageCreateInfo(m_HzbImage.ImageFormat,
                              VK_IMAGE_USAGE_STORAGE_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              m_HzbImage.ImageExtent);
  HzbInfo.mipLevels = MipCount;

  VmaAllocationCreateInfo AllocationInfo{};
  AllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  AllocationInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK(vmaCreateImage(m_Device.Allocator, &HzbInfo, &AllocationInfo,
                          &m_HzbImage.Image, &m_HzbImage.Allocation,
                          VK_NULL_HANDLE));

  m_HzbMipImageViews.reserve(MipCount);
  m_HzbMipExtents.reserve(MipCount);
  m_HzbMipOffsets.reserve(MipCount);

  for (uint32_t MipLevel = 0; MipLevel < MipCount; ++MipLevel) {
    const VkExtent2D MipExtent = ComputeHzbMipExtent(BaseExtent, MipLevel);
    m_HzbMipExtents.push_back(MipExtent);
    m_HzbMipOffsets.push_back(m_HzbReadbackBufferSize);
    m_HzbReadbackBufferSize +=
        static_cast<VkDeviceSize>(MipExtent.width) *
        static_cast<VkDeviceSize>(MipExtent.height) * sizeof(float);

    VkImageViewCreateInfo ViewInfo = VkInit::ImageViewCreateInfo(
        m_HzbImage.ImageFormat, m_HzbImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
    ViewInfo.subresourceRange.baseMipLevel = MipLevel;
    ViewInfo.subresourceRange.levelCount = 1;

    VkImageView MipView = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(m_Device.Device, &ViewInfo, VK_NULL_HANDLE,
                               &MipView));
    m_HzbMipImageViews.push_back(MipView);
  }

  m_MainDeletionQueue.PushFunction([this]() {
    for (VkImageView MipView : m_HzbMipImageViews) {
      if (MipView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_Device.Device, MipView, VK_NULL_HANDLE);
      }
    }
    m_HzbMipImageViews.clear();
    vmaDestroyImage(m_Device.Allocator, m_HzbImage.Image, m_HzbImage.Allocation);
    m_HzbImage = {};
  });
}

void VulkanRendererBackend::Shutdown() {
  A_CORE_INFO("Running Vulkan renderer cleanup...");

  if (m_IsInitialized) {
    vkDeviceWaitIdle(m_Device.Device);
    m_MaterialResources.Shutdown();

    for (auto &Frame : m_MeshFrames) {
      VkBufferUtil::DestroyBuffer(m_Device.Allocator, Frame.CameraBuffer);
      VkBufferUtil::DestroyBuffer(m_Device.Allocator, Frame.HzbReadbackBuffer);
    }

    m_CommandContext.Shutdown(m_Device.Device);
    m_MainDeletionQueue.Flush();
    m_Swapchain.Shutdown(m_Device);
    m_Device.Shutdown();
    m_Context.Shutdown();

    m_IsInitialized = false;
  }

  g_LoadedEngine = nullptr;
}

void VulkanRendererBackend::InitDescriptors() {
  const uint32_t MaxSets = 4 +
                           (FRAME_OVERLAP * (MaxMeshSubmissionsPerFrame + 2)) +
                           (MaxMeshSubmissionsPerFrame * 2) +
                           static_cast<uint32_t>(m_HzbMipImageViews.size());
  std::vector<DescriptorAllocator::PoolSizeRatio> Sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4.0f},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6.0f},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2.0f},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 2.0f},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4.0f},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.0f}};

  m_GlobalDescriptorAllocator.InitPool(m_Device.Device, MaxSets, Sizes);

  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_DrawImageDescriptorLayout =
        Builder.Build(m_Device.Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_HzbReduceDescriptorLayout =
        Builder.Build(m_Device.Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    Builder.AddBinding(2, VK_DESCRIPTOR_TYPE_SAMPLER);
    m_MeshGraphicsFrameDescriptorLayout =
        Builder.Build(m_Device.Device,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Builder.AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_MeshComputeFrameDescriptorLayout =
        Builder.Build(m_Device.Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    Builder.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    m_MeshDescriptorLayout =
        Builder.Build(m_Device.Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  m_DrawImageDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
      m_Device.Device, m_DrawImageDescriptorLayout);

  VkDescriptorImageInfo ImageInfo{};
  ImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  ImageInfo.imageView = m_DrawImage.ImageView;

  VkWriteDescriptorSet DrawImageWrite = VkInit::WriteDescriptorSet(
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_DrawImageDescriptorSet, &ImageInfo, 0);

  vkUpdateDescriptorSets(m_Device.Device, 1, &DrawImageWrite, 0,
                         VK_NULL_HANDLE);

  VkSamplerCreateInfo SamplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .magFilter = VK_FILTER_NEAREST,
      .minFilter = VK_FILTER_NEAREST,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .compareEnable = VK_FALSE,
      .minLod = 0.0f,
      .maxLod = 0.0f,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
      .unnormalizedCoordinates = VK_FALSE};
  VK_CHECK(vkCreateSampler(m_Device.Device, &SamplerInfo, VK_NULL_HANDLE,
                           &m_LinearDepthSampler));

  SamplerInfo.magFilter = VK_FILTER_LINEAR;
  SamplerInfo.minFilter = VK_FILTER_LINEAR;
  SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  SamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  SamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  SamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VK_CHECK(vkCreateSampler(m_Device.Device, &SamplerInfo, VK_NULL_HANDLE,
                           &m_TextureSampler));

  m_HzbReduceDescriptorSets.clear();
  m_HzbReduceDescriptorSets.reserve(m_HzbMipImageViews.size());
  for (size_t MipLevel = 0; MipLevel < m_HzbMipImageViews.size(); ++MipLevel) {
    VkDescriptorSet DescriptorSet = m_GlobalDescriptorAllocator.Allocate(
        m_Device.Device, m_HzbReduceDescriptorLayout);
    m_HzbReduceDescriptorSets.push_back(DescriptorSet);

    VkDescriptorImageInfo DestinationImageInfo{};
    DestinationImageInfo.imageView = m_HzbMipImageViews[MipLevel];
    DestinationImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo SourceImageInfo{};
    SourceImageInfo.sampler = m_LinearDepthSampler;
    SourceImageInfo.imageView =
        (MipLevel == 0) ? m_RasterDepthImage.ImageView
                        : m_HzbMipImageViews[MipLevel - 1];
    SourceImageInfo.imageLayout =
        (MipLevel == 0) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                        : VK_IMAGE_LAYOUT_GENERAL;

    std::array<VkWriteDescriptorSet, 2> Writes = {
        VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   DescriptorSet, &DestinationImageInfo, 0),
        VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   DescriptorSet, &SourceImageInfo, 1)};
    vkUpdateDescriptorSets(m_Device.Device, static_cast<uint32_t>(Writes.size()),
                           Writes.data(), 0, VK_NULL_HANDLE);
  }

  m_MainDeletionQueue.PushFunction([this]() {
    vkDestroySampler(m_Device.Device, m_TextureSampler, VK_NULL_HANDLE);
    vkDestroySampler(m_Device.Device, m_LinearDepthSampler, VK_NULL_HANDLE);
    m_GlobalDescriptorAllocator.DestroyPool(m_Device.Device);
    vkDestroyDescriptorSetLayout(m_Device.Device, m_MeshDescriptorLayout,
                                 VK_NULL_HANDLE);
    vkDestroyDescriptorSetLayout(m_Device.Device,
                                 m_MeshComputeFrameDescriptorLayout,
                                 VK_NULL_HANDLE);
    vkDestroyDescriptorSetLayout(m_Device.Device,
                                 m_MeshGraphicsFrameDescriptorLayout,
                                 VK_NULL_HANDLE);
    vkDestroyDescriptorSetLayout(m_Device.Device, m_HzbReduceDescriptorLayout,
                                 VK_NULL_HANDLE);
    vkDestroyDescriptorSetLayout(m_Device.Device, m_DrawImageDescriptorLayout,
                                 VK_NULL_HANDLE);
  });
}

void VulkanRendererBackend::InitTextureResources() {
  m_MaterialResources.InitFallbackTexture();
  const std::filesystem::path LightIconPath =
      std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine" / "lightbulb.svg";
  if (const auto IconTexture = Assets::LoadSvgTextureFromFile(LightIconPath)) {
    m_LightBillboardMaterial = std::make_shared<MaterialInstance>();
    m_LightBillboardMaterial->BaseColorTexture = IconTexture;
  } else {
    A_CORE_WARN("Failed to load light billboard icon from {0}; using fallback texture",
                LightIconPath.string());
    m_LightBillboardMaterial = std::make_shared<MaterialInstance>();
  }
}

void VulkanRendererBackend::InitPipelines() {
  InitBackgroundPipelines();
  InitMeshPipelines();
  InitGizmoPipeline();
  InitLightBillboardPipeline();
}

void VulkanRendererBackend::InitGizmoPipeline() {
  m_GizmoRenderer.Init({.Device = m_Device.Device,
                         .DrawImageFormat = m_DrawImage.ImageFormat},
                        m_MainDeletionQueue);
}

void VulkanRendererBackend::InitLightBillboardPipeline() {
  const VkImageView TextureView =
      m_MaterialResources.ResolveMaterialTextureView(m_LightBillboardMaterial);
  m_LightBillboardRenderer.Init(
      {.Device = m_Device.Device,
       .DrawImageFormat = m_DrawImage.ImageFormat,
       .DescriptorAllocator = &m_GlobalDescriptorAllocator,
       .TextureView = TextureView,
       .TextureSampler = m_TextureSampler},
      m_MainDeletionQueue);
}

void VulkanRendererBackend::InitBackgroundPipelines() {
  VkPipelineLayoutCreateInfo ComputeLayout =
      VkInit::PipelineLayoutCreateInfo();
  ComputeLayout.pSetLayouts = &m_DrawImageDescriptorLayout;
  ComputeLayout.setLayoutCount = 1;

  VkPushConstantRange PushConstant{};
  PushConstant.offset = 0;
  PushConstant.size = sizeof(ComputePushConstants);
  PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  ComputeLayout.pPushConstantRanges = &PushConstant;
  ComputeLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &ComputeLayout,
                                  VK_NULL_HANDLE, &m_GradientPipelineLayout));

  VkShaderModule ComputeDrawShader;
  const std::string ShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/gradient_color.comp.spv";
  if (!VkUtil::LoadShaderModule(ShaderPath.c_str(), m_Device.Device,
                                &ComputeDrawShader)) {
    A_ERROR("Error when loading the compute shader: {0}", ShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  VkPipelineShaderStageCreateInfo StageInfo =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            ComputeDrawShader);

  VkComputePipelineCreateInfo ComputePipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .stage = StageInfo,
      .layout = m_GradientPipelineLayout};

  VK_CHECK(vkCreateComputePipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                    &ComputePipelineCreateInfo,
                                    VK_NULL_HANDLE, &m_GradientPipeline));

  vkDestroyShaderModule(m_Device.Device, ComputeDrawShader, VK_NULL_HANDLE);

  m_MainDeletionQueue.PushFunction([this]() {
    vkDestroyPipelineLayout(m_Device.Device, m_GradientPipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_GradientPipeline, VK_NULL_HANDLE);
  });

  // --- HDR skybox pipeline -------------------------------------------------
  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_HDRSkyboxDescriptorLayout =
        Builder.Build(m_Device.Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  VkSamplerCreateInfo HDRSamplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .compareEnable = VK_FALSE,
      .minLod = 0.0f,
      .maxLod = 0.0f,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE};
  VK_CHECK(vkCreateSampler(m_Device.Device, &HDRSamplerInfo, VK_NULL_HANDLE,
                           &m_HDRSkyboxSampler));

  const std::array<VkDescriptorSetLayout, 2> HDRSetLayouts = {
      m_DrawImageDescriptorLayout, m_HDRSkyboxDescriptorLayout};

  VkPipelineLayoutCreateInfo HDRLayout = VkInit::PipelineLayoutCreateInfo();
  HDRLayout.pSetLayouts = HDRSetLayouts.data();
  HDRLayout.setLayoutCount = static_cast<uint32_t>(HDRSetLayouts.size());

  VkPushConstantRange HDRPushConstant{};
  HDRPushConstant.offset = 0;
  HDRPushConstant.size = sizeof(glm::mat4);
  HDRPushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  HDRLayout.pPushConstantRanges = &HDRPushConstant;
  HDRLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &HDRLayout, VK_NULL_HANDLE,
                                  &m_HDRSkyboxPipelineLayout));

  VkShaderModule HDRShader;
  const std::string HDRShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/skybox_hdr.comp.spv";
  if (!VkUtil::LoadShaderModule(HDRShaderPath.c_str(), m_Device.Device,
                                &HDRShader)) {
    A_ERROR("Error when loading the HDR skybox compute shader: {0}",
            HDRShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  VkPipelineShaderStageCreateInfo HDRStage =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            HDRShader);

  VkComputePipelineCreateInfo HDRPipelineInfo = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .stage = HDRStage,
      .layout = m_HDRSkyboxPipelineLayout};

  VK_CHECK(vkCreateComputePipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                    &HDRPipelineInfo, VK_NULL_HANDLE,
                                    &m_HDRSkyboxPipeline));

  vkDestroyShaderModule(m_Device.Device, HDRShader, VK_NULL_HANDLE);

  m_MainDeletionQueue.PushFunction([this]() {
    DestroyHDRSkyboxTexture();
    if (m_HDRSkyboxSampler != VK_NULL_HANDLE) {
      vkDestroySampler(m_Device.Device, m_HDRSkyboxSampler, VK_NULL_HANDLE);
      m_HDRSkyboxSampler = VK_NULL_HANDLE;
    }
    vkDestroyPipeline(m_Device.Device, m_HDRSkyboxPipeline, VK_NULL_HANDLE);
    vkDestroyPipelineLayout(m_Device.Device, m_HDRSkyboxPipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyDescriptorSetLayout(m_Device.Device, m_HDRSkyboxDescriptorLayout,
                                 VK_NULL_HANDLE);
  });
}

void VulkanRendererBackend::InitMeshPipelines() {
  std::array<VkDescriptorSetLayout, 2> ComputeLayouts = {
      m_MeshComputeFrameDescriptorLayout, m_MeshDescriptorLayout};

  {
    VkPipelineLayoutCreateInfo ComputeLayout =
        VkInit::PipelineLayoutCreateInfo();
    ComputeLayout.pSetLayouts = &m_HzbReduceDescriptorLayout;
    ComputeLayout.setLayoutCount = 1;

    VkPushConstantRange PushConstant{};
    PushConstant.offset = 0;
    PushConstant.size = sizeof(HzbReducePushConstants);
    PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    ComputeLayout.pPushConstantRanges = &PushConstant;
    ComputeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &ComputeLayout,
                                    VK_NULL_HANDLE,
                                    &m_HzbReducePipelineLayout));
  }

  {
    VkPipelineLayoutCreateInfo ComputeLayout =
        VkInit::PipelineLayoutCreateInfo();
    ComputeLayout.pSetLayouts = ComputeLayouts.data();
    ComputeLayout.setLayoutCount =
        static_cast<uint32_t>(ComputeLayouts.size());

    VkPushConstantRange PushConstant{};
    PushConstant.offset = 0;
    PushConstant.size = sizeof(MeshProjectPushConstants);
    PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    ComputeLayout.pPushConstantRanges = &PushConstant;
    ComputeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &ComputeLayout,
                                    VK_NULL_HANDLE,
                                    &m_MeshProjectPipelineLayout));
  }

  {
    VkPipelineLayoutCreateInfo ComputeLayout =
        VkInit::PipelineLayoutCreateInfo();
    ComputeLayout.pSetLayouts = ComputeLayouts.data();
    ComputeLayout.setLayoutCount =
        static_cast<uint32_t>(ComputeLayouts.size());

    VkPushConstantRange PushConstant{};
    PushConstant.offset = 0;
    PushConstant.size = sizeof(MeshRasterPushConstants);
    PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    ComputeLayout.pPushConstantRanges = &PushConstant;
    ComputeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &ComputeLayout,
                                    VK_NULL_HANDLE, &m_MeshPipelineLayout));
  }

  VkPushConstantRange PushConstant{};
  PushConstant.offset = 0;
  PushConstant.size = sizeof(MeshGraphicsPushConstants);
  PushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  VkPipelineLayoutCreateInfo GraphicsLayout = VkInit::PipelineLayoutCreateInfo();
  GraphicsLayout.pSetLayouts = &m_MeshGraphicsFrameDescriptorLayout;
  GraphicsLayout.setLayoutCount = 1;
  GraphicsLayout.pPushConstantRanges = &PushConstant;
  GraphicsLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &GraphicsLayout,
                                  VK_NULL_HANDLE,
                                  &m_MeshGraphicsPipelineLayout));

  VkPipelineLayoutCreateInfo DepthLayout = VkInit::PipelineLayoutCreateInfo();
  DepthLayout.pSetLayouts = &m_MeshGraphicsFrameDescriptorLayout;
  DepthLayout.setLayoutCount = 1;
  DepthLayout.pPushConstantRanges = &PushConstant;
  DepthLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &DepthLayout,
                                  VK_NULL_HANDLE, &m_MeshDepthPipelineLayout));

  VkShaderModule HzbReduceShader;
  const std::string HzbReduceShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/hzb_reduce.comp.spv";
  if (!VkUtil::LoadShaderModule(HzbReduceShaderPath.c_str(), m_Device.Device,
                                &HzbReduceShader)) {
    A_ERROR("Error when loading the HZB reduction shader: {0}",
            HzbReduceShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  VkPipelineShaderStageCreateInfo HzbReduceStageInfo =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            HzbReduceShader);

  VkComputePipelineCreateInfo HzbReducePipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .stage = HzbReduceStageInfo,
      .layout = m_HzbReducePipelineLayout};

  VK_CHECK(vkCreateComputePipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                    &HzbReducePipelineCreateInfo,
                                    VK_NULL_HANDLE, &m_HzbReducePipeline));
  vkDestroyShaderModule(m_Device.Device, HzbReduceShader, VK_NULL_HANDLE);

  VkShaderModule MeshProjectShader;
  const std::string MeshProjectShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh_project.comp.spv";
  if (!VkUtil::LoadShaderModule(MeshProjectShaderPath.c_str(), m_Device.Device,
                                &MeshProjectShader)) {
    A_ERROR("Error when loading the mesh projection shader: {0}",
            MeshProjectShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  VkPipelineShaderStageCreateInfo MeshProjectStageInfo =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            MeshProjectShader);

  VkComputePipelineCreateInfo MeshProjectPipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .stage = MeshProjectStageInfo,
      .layout = m_MeshProjectPipelineLayout};

  VK_CHECK(vkCreateComputePipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                    &MeshProjectPipelineCreateInfo,
                                    VK_NULL_HANDLE, &m_MeshProjectPipeline));
  vkDestroyShaderModule(m_Device.Device, MeshProjectShader, VK_NULL_HANDLE);

  VkShaderModule MeshShader;
  const std::string MeshShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh_raster.comp.spv";
  if (!VkUtil::LoadShaderModule(MeshShaderPath.c_str(), m_Device.Device,
                                &MeshShader)) {
    A_ERROR("Error when loading the mesh compute shader: {0}", MeshShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  VkPipelineShaderStageCreateInfo MeshStageInfo =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            MeshShader);

  VkComputePipelineCreateInfo MeshPipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .stage = MeshStageInfo,
      .layout = m_MeshPipelineLayout};

  VK_CHECK(vkCreateComputePipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                    &MeshPipelineCreateInfo, VK_NULL_HANDLE,
                                    &m_MeshPipeline));
  vkDestroyShaderModule(m_Device.Device, MeshShader, VK_NULL_HANDLE);

  VkShaderModule VertexShader;
  const std::string VertexShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh.vert.spv";
  if (!VkUtil::LoadShaderModule(VertexShaderPath.c_str(), m_Device.Device,
                                &VertexShader)) {
    A_ERROR("Error when loading the mesh vertex shader: {0}", VertexShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  VkShaderModule FragmentShader;
  const std::string FragmentShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh.frag.spv";
  if (!VkUtil::LoadShaderModule(FragmentShaderPath.c_str(), m_Device.Device,
                                &FragmentShader)) {
    A_ERROR("Error when loading the mesh fragment shader: {0}",
            FragmentShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages = {
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            VertexShader),
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            FragmentShader)};

  VkVertexInputBindingDescription BindingDescription{
      .binding = 0,
      .stride = sizeof(MeshVertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
  std::array<VkVertexInputAttributeDescription, 3> AttributeDescriptions = {
      VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                        offsetof(MeshVertex, Position)},
      VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                        offsetof(MeshVertex, Normal)},
      VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32_SFLOAT,
                                        offsetof(MeshVertex, TexCoord)}};

  VkPipelineVertexInputStateCreateInfo VertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &BindingDescription,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(AttributeDescriptions.size()),
      .pVertexAttributeDescriptions = AttributeDescriptions.data()};

  VkPipelineInputAssemblyStateCreateInfo InputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE};

  VkPipelineViewportStateCreateInfo ViewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .viewportCount = 1,
      .scissorCount = 1};

  VkPipelineRasterizationStateCreateInfo Rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f};

  VkPipelineMultisampleStateCreateInfo Multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE};

  VkPipelineDepthStencilStateCreateInfo DepthStencil{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE};

  VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
  ColorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  ColorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo ColorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &ColorBlendAttachment};

  std::array<VkDynamicState, 2> DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo DynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .dynamicStateCount = static_cast<uint32_t>(DynamicStates.size()),
      .pDynamicStates = DynamicStates.data()};

  VkPipelineRenderingCreateInfo RenderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &m_DrawImage.ImageFormat,
      .depthAttachmentFormat = m_RasterDepthImage.ImageFormat,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED};

  VkGraphicsPipelineCreateInfo PipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &RenderingInfo,
      .stageCount = static_cast<uint32_t>(ShaderStages.size()),
      .pStages = ShaderStages.data(),
      .pVertexInputState = &VertexInputInfo,
      .pInputAssemblyState = &InputAssembly,
      .pViewportState = &ViewportState,
      .pRasterizationState = &Rasterizer,
      .pMultisampleState = &Multisampling,
      .pDepthStencilState = &DepthStencil,
      .pColorBlendState = &ColorBlending,
      .pDynamicState = &DynamicState,
      .layout = m_MeshGraphicsPipelineLayout,
      .renderPass = VK_NULL_HANDLE,
      .subpass = 0};

  VK_CHECK(vkCreateGraphicsPipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                     &PipelineInfo, VK_NULL_HANDLE,
                                     &m_MeshGraphicsPipeline));

  VkPipelineColorBlendAttachmentState AlphaBlendAttachment = ColorBlendAttachment;
  AlphaBlendAttachment.blendEnable = VK_TRUE;
  AlphaBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  AlphaBlendAttachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  AlphaBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  AlphaBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  AlphaBlendAttachment.dstAlphaBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  AlphaBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  VkPipelineColorBlendStateCreateInfo AlphaBlending = ColorBlending;
  AlphaBlending.pAttachments = &AlphaBlendAttachment;
  VkPipelineDepthStencilStateCreateInfo AlphaDepthStencil = DepthStencil;
  AlphaDepthStencil.depthWriteEnable = VK_FALSE;
  VkGraphicsPipelineCreateInfo AlphaPipelineInfo = PipelineInfo;
  AlphaPipelineInfo.pColorBlendState = &AlphaBlending;
  AlphaPipelineInfo.pDepthStencilState = &AlphaDepthStencil;
  VK_CHECK(vkCreateGraphicsPipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                     &AlphaPipelineInfo, VK_NULL_HANDLE,
                                     &m_MeshGraphicsAlphaBlendPipeline));

  VkPipelineColorBlendAttachmentState DepthOnlyColorAttachment{};
  DepthOnlyColorAttachment.colorWriteMask = 0;
  VkPipelineColorBlendStateCreateInfo DepthOnlyBlending = ColorBlending;
  DepthOnlyBlending.pAttachments = &DepthOnlyColorAttachment;

  VkPipelineRenderingCreateInfo DepthRenderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .colorAttachmentCount = 0,
      .pColorAttachmentFormats = VK_NULL_HANDLE,
      .depthAttachmentFormat = m_RasterDepthImage.ImageFormat,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED};
  VkGraphicsPipelineCreateInfo DepthPipelineInfo = PipelineInfo;
  DepthPipelineInfo.pNext = &DepthRenderingInfo;
  DepthPipelineInfo.pColorBlendState = &DepthOnlyBlending;
  DepthPipelineInfo.layout = m_MeshDepthPipelineLayout;
  DepthPipelineInfo.stageCount = 1;
  DepthPipelineInfo.pStages = &ShaderStages[0];
  VK_CHECK(vkCreateGraphicsPipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                     &DepthPipelineInfo, VK_NULL_HANDLE,
                                     &m_MeshDepthPipeline));

  VkPipelineRasterizationStateCreateInfo WireframeRasterizer = Rasterizer;
  WireframeRasterizer.polygonMode = VK_POLYGON_MODE_LINE;
  VkPipelineDepthStencilStateCreateInfo WireframeDepthStencil = DepthStencil;
  WireframeDepthStencil.depthTestEnable = VK_FALSE;
  WireframeDepthStencil.depthWriteEnable = VK_FALSE;
  VkGraphicsPipelineCreateInfo WireframePipelineInfo = PipelineInfo;
  WireframePipelineInfo.pRasterizationState = &WireframeRasterizer;
  WireframePipelineInfo.pDepthStencilState = &WireframeDepthStencil;
  VK_CHECK(vkCreateGraphicsPipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                     &WireframePipelineInfo, VK_NULL_HANDLE,
                                     &m_MeshWireframePipeline));

  vkDestroyShaderModule(m_Device.Device, VertexShader, VK_NULL_HANDLE);
  vkDestroyShaderModule(m_Device.Device, FragmentShader, VK_NULL_HANDLE);

  m_MainDeletionQueue.PushFunction([this]() {
    vkDestroyPipelineLayout(m_Device.Device, m_HzbReducePipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_HzbReducePipeline, VK_NULL_HANDLE);
    vkDestroyPipelineLayout(m_Device.Device, m_MeshProjectPipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_MeshProjectPipeline, VK_NULL_HANDLE);
    vkDestroyPipelineLayout(m_Device.Device, m_MeshPipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_MeshPipeline, VK_NULL_HANDLE);
    vkDestroyPipelineLayout(m_Device.Device, m_MeshDepthPipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_MeshDepthPipeline, VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_MeshWireframePipeline,
                      VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_MeshGraphicsAlphaBlendPipeline,
                      VK_NULL_HANDLE);
    vkDestroyPipelineLayout(m_Device.Device, m_MeshGraphicsPipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_MeshGraphicsPipeline, VK_NULL_HANDLE);
  });
}

void VulkanRendererBackend::InitMeshFrameResources() {
  for (auto &Frame : m_MeshFrames) {
    Frame.CameraBuffer = VkBufferUtil::CreateBuffer(
        m_Device.Allocator, sizeof(CameraFrameUniform),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);
    Frame.HzbReadbackBuffer = VkBufferUtil::CreateBuffer(
        m_Device.Allocator, static_cast<size_t>(m_HzbReadbackBufferSize),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);

    Frame.DepthFrameDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
        m_Device.Device, m_MeshGraphicsFrameDescriptorLayout);
    for (VkDescriptorSet &DescriptorSet : Frame.GraphicsFrameDescriptorSets) {
      DescriptorSet = m_GlobalDescriptorAllocator.Allocate(
          m_Device.Device, m_MeshGraphicsFrameDescriptorLayout);
    }
    Frame.ComputeFrameDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
        m_Device.Device, m_MeshComputeFrameDescriptorLayout);

    VkQueryPoolCreateInfo QueryPoolInfo{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = TimestampQueryCount,
        .pipelineStatistics = 0};
    VK_CHECK(vkCreateQueryPool(m_Device.Device, &QueryPoolInfo, VK_NULL_HANDLE,
                               &Frame.TimestampQueryPool));
  }

  m_MainDeletionQueue.PushFunction([this]() {
    for (auto &Frame : m_MeshFrames) {
      if (Frame.TimestampQueryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(m_Device.Device, Frame.TimestampQueryPool,
                           VK_NULL_HANDLE);
        Frame.TimestampQueryPool = VK_NULL_HANDLE;
      }
    }
  });
}

std::shared_ptr<Mesh>
VulkanRendererBackend::CreateMesh(const MeshData &MeshSource) {
  return VulkanMesh::Create(MeshSource, m_Device.Allocator, m_Device.Device,
                            m_Device.GraphicsQueue, GetCurrentFrame().CommandPool,
                            m_GlobalDescriptorAllocator, m_MeshDescriptorLayout);
}

void VulkanRendererBackend::CollectFrameStats(MeshFrameResources &Frame) {
  if (!Frame.HasValidTimestamps) {
    return;
  }

  uint64_t Timestamps[TimestampQueryCount] = {};
  const VkResult Result = vkGetQueryPoolResults(
      m_Device.Device, Frame.TimestampQueryPool, 0, TimestampQueryCount,
      sizeof(Timestamps), Timestamps, sizeof(uint64_t),
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  if (Result != VK_SUCCESS) {
    return;
  }

  const auto ToMilliseconds = [this](uint64_t Start, uint64_t End) {
    if (End <= Start) {
      return 0.0f;
    }

    const double Nanoseconds =
        static_cast<double>(End - Start) * static_cast<double>(m_TimestampPeriod);
    return static_cast<float>(Nanoseconds / 1'000'000.0);
  };

  m_FrameStats.GpuBackgroundMs = ToMilliseconds(Timestamps[0], Timestamps[1]);
  m_FrameStats.GpuMeshMs = ToMilliseconds(Timestamps[2], Timestamps[3]);
}

void VulkanRendererBackend::DrawBackground(VkCommandBuffer CommandBuffer) {
  SyncHDRSkyboxTexture();

  const bool UseHDR = m_LoadedHDRSkyboxData != nullptr &&
                      m_HDRSkyboxDescriptorSet != VK_NULL_HANDLE &&
                      m_ActiveScene != nullptr &&
                      m_ActiveScene->ActiveCamera != nullptr;

  if (UseHDR) {
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_HDRSkyboxPipeline);
    const std::array<VkDescriptorSet, 2> Sets = {m_DrawImageDescriptorSet,
                                                 m_HDRSkyboxDescriptorSet};
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_HDRSkyboxPipelineLayout, 0,
                            static_cast<uint32_t>(Sets.size()), Sets.data(), 0,
                            VK_NULL_HANDLE);

    const glm::mat4 InverseViewProj = glm::inverse(
        m_ActiveScene->ActiveCamera->GetViewProjectionMatrix());

    vkCmdPushConstants(CommandBuffer, m_HDRSkyboxPipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::mat4),
                       glm::value_ptr(InverseViewProj));

    vkCmdDispatch(CommandBuffer,
                  static_cast<uint32_t>(std::ceil(m_DrawExtent.width / 16.0f)),
                  static_cast<uint32_t>(std::ceil(m_DrawExtent.height / 16.0f)),
                  1);
    return;
  }

  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    m_GradientPipeline);
  vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_GradientPipelineLayout, 0, 1,
                          &m_DrawImageDescriptorSet, 0, VK_NULL_HANDLE);

  ComputePushConstants PC;
  if (m_ActiveScene) {
    PC.data1 = glm::vec4(m_ActiveScene->SkyboxColorTop, 1.0f);
    PC.data2 = glm::vec4(m_ActiveScene->SkyboxColorBottom, 1.0f);
  } else {
    PC.data1 = glm::vec4(0.08f, 0.09f, 0.14f, 1.0f);
    PC.data2 = glm::vec4(0.14f, 0.24f, 0.38f, 1.0f);
  }

  vkCmdPushConstants(CommandBuffer, m_GradientPipelineLayout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0,
                     sizeof(ComputePushConstants), &PC);

  vkCmdDispatch(CommandBuffer,
                static_cast<uint32_t>(std::ceil(m_DrawExtent.width / 16.0f)),
                static_cast<uint32_t>(std::ceil(m_DrawExtent.height / 16.0f)), 1);
}

void VulkanRendererBackend::SyncHDRSkyboxTexture() {
  const HDRTextureSourceDataRef Wanted =
      m_ActiveScene ? m_ActiveScene->SkyboxHDRTexture : nullptr;
  if (Wanted == m_LoadedHDRSkyboxData) {
    return;
  }

  // Defer destruction of the previous image onto the current frame's deletion
  // queue. That queue flushes when this frame slot is reused, after its
  // RenderFence is signaled — guaranteeing every command buffer that bound the
  // old image/descriptor has finished executing on the GPU.
  if (m_HDRSkyboxImage.Image != VK_NULL_HANDLE) {
    AllocatedImage OldImage = m_HDRSkyboxImage;
    GetCurrentFrame().DeletionQueue.PushFunction(
        [Device = m_Device.Device, Allocator = m_Device.Allocator,
         OldImage]() mutable {
          if (OldImage.ImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(Device, OldImage.ImageView, VK_NULL_HANDLE);
          }
          if (OldImage.Image != VK_NULL_HANDLE) {
            vmaDestroyImage(Allocator, OldImage.Image, OldImage.Allocation);
          }
        });
    m_HDRSkyboxImage = {};
    // The descriptor set is pool-allocated; drop the handle. The pool itself
    // is destroyed at shutdown.
    m_HDRSkyboxDescriptorSet = VK_NULL_HANDLE;
  }

  if (Wanted) {
    UploadHDRSkyboxTexture(*Wanted);
  }
  m_LoadedHDRSkyboxData = Wanted;
}

void VulkanRendererBackend::UploadHDRSkyboxTexture(
    const HDRTextureSourceData &Texture) {
  if (!Texture.IsValid()) {
    A_CORE_WARN("HDR skybox: refusing to upload invalid texture ({}x{})",
                Texture.Width, Texture.Height);
    return;
  }

  m_HDRSkyboxImage.ImageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
  m_HDRSkyboxImage.ImageExtent = {Texture.Width, Texture.Height, 1};

  VkImageCreateInfo ImageInfo = VkInit::ImageCreateInfo(
      m_HDRSkyboxImage.ImageFormat,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      m_HDRSkyboxImage.ImageExtent);

  VmaAllocationCreateInfo AllocationInfo{};
  AllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  AllocationInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VK_CHECK(vmaCreateImage(m_Device.Allocator, &ImageInfo, &AllocationInfo,
                          &m_HDRSkyboxImage.Image, &m_HDRSkyboxImage.Allocation,
                          VK_NULL_HANDLE));

  VkImageViewCreateInfo ViewInfo = VkInit::ImageViewCreateInfo(
      m_HDRSkyboxImage.ImageFormat, m_HDRSkyboxImage.Image,
      VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device.Device, &ViewInfo, VK_NULL_HANDLE,
                             &m_HDRSkyboxImage.ImageView));

  const VkDeviceSize ByteCount =
      static_cast<VkDeviceSize>(Texture.Pixels.size()) * sizeof(float);
  auto StagingBuffer = VkBufferUtil::CreateBuffer(
      m_Device.Allocator, ByteCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_CPU_ONLY,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT);
  std::memcpy(StagingBuffer.Info.pMappedData, Texture.Pixels.data(), ByteCount);

  ImmediateSubmit([&](VkCommandBuffer CommandBuffer) {
    VkUtil::TransitionImage(CommandBuffer, m_HDRSkyboxImage.Image,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy Region{};
    Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Region.imageSubresource.layerCount = 1;
    Region.imageExtent = m_HDRSkyboxImage.ImageExtent;
    vkCmdCopyBufferToImage(CommandBuffer, StagingBuffer.Buffer,
                           m_HDRSkyboxImage.Image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

    VkUtil::TransitionImage(CommandBuffer, m_HDRSkyboxImage.Image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  });

  VkBufferUtil::DestroyBuffer(m_Device.Allocator, StagingBuffer);

  m_HDRSkyboxDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
      m_Device.Device, m_HDRSkyboxDescriptorLayout);

  VkDescriptorImageInfo SamplerImage{};
  SamplerImage.sampler = m_HDRSkyboxSampler;
  SamplerImage.imageView = m_HDRSkyboxImage.ImageView;
  SamplerImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkWriteDescriptorSet Write = VkInit::WriteDescriptorSet(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_HDRSkyboxDescriptorSet,
      &SamplerImage, 0);
  vkUpdateDescriptorSets(m_Device.Device, 1, &Write, 0, VK_NULL_HANDLE);
}

void VulkanRendererBackend::DestroyHDRSkyboxTexture() {
  if (m_HDRSkyboxImage.ImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(m_Device.Device, m_HDRSkyboxImage.ImageView,
                       VK_NULL_HANDLE);
    m_HDRSkyboxImage.ImageView = VK_NULL_HANDLE;
  }
  if (m_HDRSkyboxImage.Image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_Device.Allocator, m_HDRSkyboxImage.Image,
                    m_HDRSkyboxImage.Allocation);
    m_HDRSkyboxImage.Image = VK_NULL_HANDLE;
    m_HDRSkyboxImage.Allocation = VK_NULL_HANDLE;
  }
  // The descriptor set lives in the pool; we drop the handle and rely on the
  // pool being destroyed at shutdown.
  m_HDRSkyboxDescriptorSet = VK_NULL_HANDLE;
}

void VulkanRendererBackend::BuildHzb(VkCommandBuffer CommandBuffer,
                                     MeshFrameResources &Frame) {
  if (m_HzbReduceDescriptorSets.empty()) {
    Frame.HasValidOcclusionData = false;
    return;
  }

  VkUtil::TransitionImage(CommandBuffer, m_RasterDepthImage.Image,
                          VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
  TransitionImageRange(CommandBuffer, m_HzbImage.Image, m_HzbImageLayout,
                       VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 0,
                       static_cast<uint32_t>(m_HzbMipImageViews.size()));
  m_HzbImageLayout = VK_IMAGE_LAYOUT_GENERAL;

  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    m_HzbReducePipeline);
  for (size_t MipLevel = 0; MipLevel < m_HzbReduceDescriptorSets.size();
       ++MipLevel) {
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_HzbReducePipelineLayout, 0, 1,
                            &m_HzbReduceDescriptorSets[MipLevel], 0,
                            VK_NULL_HANDLE);

    const VkExtent2D SourceExtent =
        (MipLevel == 0) ? VkExtent2D{m_DrawExtent.width, m_DrawExtent.height}
                        : m_HzbMipExtents[MipLevel - 1];
    const VkExtent2D DestinationExtent = m_HzbMipExtents[MipLevel];
    HzbReducePushConstants PushConstants{};
    PushConstants.Dimensions = glm::uvec4(SourceExtent.width, SourceExtent.height,
                                          DestinationExtent.width,
                                          DestinationExtent.height);
    vkCmdPushConstants(CommandBuffer, m_HzbReducePipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(HzbReducePushConstants), &PushConstants);

    vkCmdDispatch(CommandBuffer,
                  static_cast<uint32_t>(
                      std::ceil(DestinationExtent.width / 8.0f)),
                  static_cast<uint32_t>(
                      std::ceil(DestinationExtent.height / 8.0f)),
                  1);

    TransitionImageRange(CommandBuffer, m_HzbImage.Image, VK_IMAGE_LAYOUT_GENERAL,
                         VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT,
                         static_cast<uint32_t>(MipLevel), 1);
  }

  TransitionImageRange(CommandBuffer, m_HzbImage.Image, VK_IMAGE_LAYOUT_GENERAL,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       VK_IMAGE_ASPECT_COLOR_BIT, 0,
                       static_cast<uint32_t>(m_HzbMipImageViews.size()));
  m_HzbImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

  for (size_t MipLevel = 0; MipLevel < m_HzbMipExtents.size(); ++MipLevel) {
    VkBufferImageCopy CopyRegion{};
    CopyRegion.bufferOffset = m_HzbMipOffsets[MipLevel];
    CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    CopyRegion.imageSubresource.mipLevel = static_cast<uint32_t>(MipLevel);
    CopyRegion.imageSubresource.layerCount = 1;
    CopyRegion.imageExtent = {m_HzbMipExtents[MipLevel].width,
                              m_HzbMipExtents[MipLevel].height, 1};
    vkCmdCopyImageToBuffer(CommandBuffer, m_HzbImage.Image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           Frame.HzbReadbackBuffer.Buffer, 1, &CopyRegion);
  }

  VkBufferMemoryBarrier2 ReadbackBarrier{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .pNext = VK_NULL_HANDLE,
      .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
      .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = Frame.HzbReadbackBuffer.Buffer,
      .offset = 0,
      .size = Frame.HzbReadbackBuffer.Size};
  VkDependencyInfo ReadbackDependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = VK_NULL_HANDLE,
      .bufferMemoryBarrierCount = 1,
      .pBufferMemoryBarriers = &ReadbackBarrier};
  vkCmdPipelineBarrier2(CommandBuffer, &ReadbackDependencyInfo);

  VkUtil::TransitionImage(CommandBuffer, m_RasterDepthImage.Image,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  Frame.HasValidOcclusionData = true;
}

void VulkanRendererBackend::ClearDepthImage(VkCommandBuffer CommandBuffer) {
  const auto PreviousLayout =
      (m_FrameNumber == 0) ? VK_IMAGE_LAYOUT_UNDEFINED
                           : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  VkUtil::TransitionImage(CommandBuffer, m_RasterDepthImage.Image,
                          PreviousLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
}

void VulkanRendererBackend::DrawMeshes(VkCommandBuffer CommandBuffer,
                                       RenderScene &Scene) {
  m_SceneRenderer.RenderScenePasses(
      {.CommandBuffer = CommandBuffer,
       .Scene = Scene,
       .Frame = GetCurrentMeshFrame(),
       .FrameStats = m_FrameStats,
       .MeshFrames = m_MeshFrames,
       .CommandContext = m_CommandContext,
       .MaterialResources = m_MaterialResources,
       .OcclusionCulling = m_OcclusionCulling,
       .Device = m_Device.Device,
       .Allocator = m_Device.Allocator,
       .FrameNumber = m_FrameNumber,
       .DrawExtent = m_DrawExtent,
       .ViewMode = m_ViewMode,
       .DrawImage = m_DrawImage,
       .RasterDepthImage = m_RasterDepthImage,
       .LinearDepthSampler = m_LinearDepthSampler,
       .TextureSampler = m_TextureSampler,
       .MeshProjectPipeline = m_MeshProjectPipeline,
       .MeshProjectPipelineLayout = m_MeshProjectPipelineLayout,
       .MeshPipeline = m_MeshPipeline,
       .MeshPipelineLayout = m_MeshPipelineLayout,
       .MeshGraphicsPipeline = m_MeshGraphicsPipeline,
       .MeshGraphicsAlphaBlendPipeline = m_MeshGraphicsAlphaBlendPipeline,
       .MeshGraphicsPipelineLayout = m_MeshGraphicsPipelineLayout,
       .MeshWireframePipeline = m_MeshWireframePipeline,
       .MeshDepthPipeline = m_MeshDepthPipeline,
       .MeshDepthPipelineLayout = m_MeshDepthPipelineLayout,
       .HzbMipExtents = m_HzbMipExtents,
       .HzbMipOffsets = m_HzbMipOffsets,
       .BuildHzb =
           [this](VkCommandBuffer DrawCommandBuffer, MeshFrameResources &Frame) {
             BuildHzb(DrawCommandBuffer, Frame);
           },
       .WarnOnce =
           [this](const char *Message) {
             if (!m_HasWarnedMeshSubmissionOverflow) {
               A_CORE_WARN("{0}", Message);
               m_HasWarnedMeshSubmissionOverflow = true;
             }
           }});
}

void VulkanRendererBackend::RecordOffscreenCapture(
    VkCommandBuffer CommandBuffer, const AllocatedBuffer &ReadbackBuffer) {
  VkBufferImageCopy CopyRegion{};
  CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  CopyRegion.imageSubresource.mipLevel = 0;
  CopyRegion.imageSubresource.layerCount = 1;
  CopyRegion.imageExtent = {m_DrawExtent.width, m_DrawExtent.height, 1};

  vkCmdCopyImageToBuffer(CommandBuffer, m_DrawImage.Image,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         ReadbackBuffer.Buffer, 1, &CopyRegion);

  VkBufferMemoryBarrier2 ReadbackBarrier{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .pNext = VK_NULL_HANDLE,
      .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
      .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = ReadbackBuffer.Buffer,
      .offset = 0,
      .size = ReadbackBuffer.Size};
  VkDependencyInfo ReadbackDependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = VK_NULL_HANDLE,
      .bufferMemoryBarrierCount = 1,
      .pBufferMemoryBarriers = &ReadbackBarrier};
  vkCmdPipelineBarrier2(CommandBuffer, &ReadbackDependencyInfo);
}

float VulkanRendererBackend::HalfToFloat(uint16_t Value) {
  const uint32_t Sign = static_cast<uint32_t>(Value & 0x8000u) << 16u;
  const uint32_t Exponent = (Value & 0x7C00u) >> 10u;
  const uint32_t Mantissa = Value & 0x03FFu;

  uint32_t ResultBits = 0;
  if (Exponent == 0u) {
    if (Mantissa == 0u) {
      ResultBits = Sign;
    } else {
      uint32_t ShiftedMantissa = Mantissa;
      uint32_t AdjustedExponent = 127u - 15u + 1u;
      while ((ShiftedMantissa & 0x0400u) == 0u) {
        ShiftedMantissa <<= 1u;
        --AdjustedExponent;
      }
      ShiftedMantissa &= 0x03FFu;
      ResultBits =
          Sign | (AdjustedExponent << 23u) | (ShiftedMantissa << 13u);
    }
  } else if (Exponent == 0x1Fu) {
    ResultBits = Sign | 0x7F800000u | (Mantissa << 13u);
  } else {
    ResultBits =
        Sign | ((Exponent + (127u - 15u)) << 23u) | (Mantissa << 13u);
  }

  return std::bit_cast<float>(ResultBits);
}

uint8_t VulkanRendererBackend::LinearToByte(float Value) {
  return static_cast<uint8_t>(
      std::round(std::clamp(Value, 0.0f, 1.0f) * 255.0f));
}

void VulkanRendererBackend::ConvertCapturedFrameToRgba8(
    const AllocatedBuffer &ReadbackBuffer, uint64_t FrameNumber) {
  if (ReadbackBuffer.Info.pMappedData == nullptr) {
    return;
  }

  vmaInvalidateAllocation(m_Device.Allocator, ReadbackBuffer.Allocation, 0,
                          ReadbackBuffer.Size);

  CapturedFrame Frame{};
  Frame.FrameIndex = FrameNumber;
  Frame.Width = m_DrawExtent.width;
  Frame.Height = m_DrawExtent.height;
  Frame.Pixels.resize(static_cast<size_t>(Frame.Width) * Frame.Height * 4u);

  const auto *Source =
      static_cast<const uint16_t *>(ReadbackBuffer.Info.pMappedData);
  auto *Destination = reinterpret_cast<uint8_t *>(Frame.Pixels.data());
  for (size_t PixelIndex = 0;
       PixelIndex <
       static_cast<size_t>(Frame.Width) * static_cast<size_t>(Frame.Height);
       ++PixelIndex) {
    const size_t SourceIndex = PixelIndex * 4u;
    const size_t DestinationIndex = PixelIndex * 4u;
    Destination[DestinationIndex + 0] =
        LinearToByte(HalfToFloat(Source[SourceIndex + 0]));
    Destination[DestinationIndex + 1] =
        LinearToByte(HalfToFloat(Source[SourceIndex + 1]));
    Destination[DestinationIndex + 2] =
        LinearToByte(HalfToFloat(Source[SourceIndex + 2]));
    Destination[DestinationIndex + 3] =
        LinearToByte(HalfToFloat(Source[SourceIndex + 3]));
  }

  m_CapturedFrame = std::move(Frame);
}

void VulkanRendererBackend::Draw() {
  auto &CurrentFrame =
      m_CommandContext.PrepareFrame(m_Device.Device, m_FrameNumber);
  auto &MeshFrame = GetCurrentMeshFrame();
  auto &CaptureFrame = m_OffscreenCaptureFrames[m_FrameNumber % FRAME_OVERLAP];

  CollectFrameStats(MeshFrame);
  m_CapturedFrame.reset();
  if (!m_HasPresentationSurface) {
    PublishCompletedOffscreenFrame(m_FrameNumber);
  }

  uint32_t SwapchainImageIndex = 0;
  if (m_HasPresentationSurface) {
    SwapchainImageIndex = m_Swapchain.AcquireNextImage(
        m_Device.Device, CurrentFrame.SwapchainSemaphore);
  }

  VkCommandBuffer CommandBuffer = CurrentFrame.MainCommandBuffer;
  VK_CHECK(vkResetCommandBuffer(CommandBuffer, 0));

  VkCommandBufferBeginInfo CommandBufferBeginInfo =
      VkInit::CommandBufferBeginInfo(
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  m_DrawExtent.width = m_DrawImage.ImageExtent.width;
  m_DrawExtent.height = m_DrawImage.ImageExtent.height;
  m_FrameStats.DrawExtent = {m_DrawExtent.width, m_DrawExtent.height};
  m_FrameStats.SubmittedMeshCount = 0;
  m_FrameStats.FrustumCulledMeshCount = 0;
  m_FrameStats.OcclusionCulledMeshCount = 0;
  m_FrameStats.MeshSubmissionCount = 0;
  m_FrameStats.TriangleCount = 0;
  MeshFrame.HasValidTimestamps = true;
  MeshFrame.HasValidOcclusionData = false;

  VK_CHECK(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo));
  vkCmdResetQueryPool(CommandBuffer, MeshFrame.TimestampQueryPool, 0,
                      TimestampQueryCount);

  VkUtil::TransitionImage(CommandBuffer, m_DrawImage.Image,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  vkCmdWriteTimestamp2(CommandBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       MeshFrame.TimestampQueryPool, 0);
  DrawBackground(CommandBuffer);
  vkCmdWriteTimestamp2(CommandBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       MeshFrame.TimestampQueryPool, 1);

  ClearDepthImage(CommandBuffer);
  VkUtil::TransitionImage(CommandBuffer, m_DrawImage.Image,
                          VK_IMAGE_LAYOUT_GENERAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  vkCmdWriteTimestamp2(CommandBuffer,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       MeshFrame.TimestampQueryPool, 2);
  if (m_ActiveScene != nullptr && !m_ActiveScene->Submissions.empty() &&
      m_ActiveScene->ActiveCamera != nullptr) {
    DrawMeshes(CommandBuffer, *m_ActiveScene);
  }
  vkCmdWriteTimestamp2(CommandBuffer,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       MeshFrame.TimestampQueryPool, 3);

  if (m_ActiveScene != nullptr && m_LightBillboardRenderer.IsInitialized()) {
    m_LightBillboardRenderer.DrawLightBillboards(
        CommandBuffer, m_DrawExtent, m_DrawImage.ImageView, *m_ActiveScene);
  }

  if (m_ActiveScene != nullptr && m_GizmoRenderer.IsInitialized()) {
    m_GizmoRenderer.DrawGizmoOverlay(CommandBuffer, m_DrawExtent,
                                     m_DrawImage.ImageView, *m_ActiveScene);
  }

  VkUtil::TransitionImage(CommandBuffer, m_DrawImage.Image,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  if (m_HasPresentationSurface) {
    VkUtil::TransitionImage(CommandBuffer, m_Swapchain.Images[SwapchainImageIndex],
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkUtil::CopyImageToImage(CommandBuffer, m_DrawImage.Image,
                             m_Swapchain.Images[SwapchainImageIndex], m_DrawExtent,
                             m_Swapchain.Extent);

    VkUtil::TransitionImage(CommandBuffer, m_Swapchain.Images[SwapchainImageIndex],
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    if (m_EnableImGui) {
      m_ImGuiRenderer.RecordDrawData(CommandBuffer, m_Swapchain.Extent,
                                     m_Swapchain.ImageViews[SwapchainImageIndex]);
    }

    VkUtil::TransitionImage(CommandBuffer, m_Swapchain.Images[SwapchainImageIndex],
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  } else {
    RecordOffscreenCapture(CommandBuffer, CaptureFrame.ReadbackBuffer);
  }

  VK_CHECK(vkEndCommandBuffer(CommandBuffer));

  VkCommandBufferSubmitInfo CommandBufferSubmitInfo =
      VkInit::CommandBufferSubmitInfo(CommandBuffer);
  if (m_HasPresentationSurface) {
    VkSemaphoreSubmitInfo SwapchainSemaphoreSubmitInfo =
        VkInit::SemaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
            CurrentFrame.SwapchainSemaphore);
    VkSemaphoreSubmitInfo RenderSemaphoreSubmitInfo =
        VkInit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                    CurrentFrame.RenderSemaphore);

    VkSubmitInfo2 Submit =
        VkInit::SubmitInfo(&CommandBufferSubmitInfo, &RenderSemaphoreSubmitInfo,
                           &SwapchainSemaphoreSubmitInfo);
    VK_CHECK(vkQueueSubmit2(m_Device.GraphicsQueue, 1, &Submit,
                            CurrentFrame.RenderFence));

    m_Swapchain.Present(m_Device.GraphicsQueue, SwapchainImageIndex,
                        CurrentFrame.RenderSemaphore);
  } else {
    VkSubmitInfo2 Submit =
        VkInit::SubmitInfo(&CommandBufferSubmitInfo, VK_NULL_HANDLE,
                           VK_NULL_HANDLE);
    VK_CHECK(vkQueueSubmit2(m_Device.GraphicsQueue, 1, &Submit,
                            CurrentFrame.RenderFence));
    CaptureFrame.HasPendingReadback = true;
    CaptureFrame.SubmittedFrameNumber = m_FrameNumber;
    CaptureFrame.SubmittedUser = m_ViewportFrameUser;
  }

  m_FrameNumber++;
}

void VulkanRendererBackend::ImmediateSubmit(
    std::function<void(VkCommandBuffer Command)> &&Function) {
  m_CommandContext.ImmediateSubmit(m_Device.Device, m_Device.GraphicsQueue,
                                   std::move(Function));
}

void VulkanRendererBackend::EnqueueDeferredDestroy(
    std::function<void()> &&Function) {
  m_MainDeletionQueue.PushFunction(std::move(Function));
}

void VulkanRendererBackend::BeginFrame() {
  m_StopRendering =
      m_HasPresentationSurface && glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED);
  m_RenderFallbackBackground = false;
  m_ActiveScene = nullptr;
  if (m_StopRendering) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return;
  }

  if (m_EnableImGui) {
    m_ImGuiRenderer.BeginFrame();
  }
}

void VulkanRendererBackend::RenderSceneMeshes(RenderScene &Scene) {
  m_ActiveScene = &Scene;
}

void VulkanRendererBackend::RenderFallbackBackground(RenderScene &Scene) {
  (void)Scene;
  m_RenderFallbackBackground = true;
}

RendererFrameStats &VulkanRendererBackend::AccessFrameStats() {
  return m_FrameStats;
}

const RendererFrameStats &VulkanRendererBackend::GetFrameStats() const {
  return m_FrameStats;
}

void VulkanRendererBackend::RenderImGui() {
  if (m_StopRendering || !m_EnableImGui) {
    return;
  }

  m_ImGuiRenderer.BuildStatsUiAndRender(m_FrameStats, m_ViewMode);
}

void VulkanRendererBackend::EndFrame() {
  if (m_StopRendering) {
    return;
  }

  Draw();
}

void VulkanRendererBackend::SetViewMode(RendererViewMode ViewMode) {
  m_ViewMode = ViewMode;
}

void VulkanRendererBackend::SetViewportFrameUser(SessionUserId User) {
  m_ViewportFrameUser = User;
}

void VulkanRendererBackend::SetViewportFrameOutput(
    IViewportFrameOutput *FrameOutput) {
  m_FrameOutput = FrameOutput;
}

std::optional<CapturedFrame> VulkanRendererBackend::ConsumeCapturedFrame() {
  std::optional<CapturedFrame> Result = std::move(m_CapturedFrame);
  m_CapturedFrame.reset();
  return Result;
}

void VulkanRendererBackend::PublishCompletedOffscreenFrame(uint64_t FrameNumber) {
  auto &CaptureFrame = m_OffscreenCaptureFrames[FrameNumber % FRAME_OVERLAP];
  if (!CaptureFrame.HasPendingReadback) {
    return;
  }

  ConvertCapturedFrameToRgba8(CaptureFrame.ReadbackBuffer,
                              CaptureFrame.SubmittedFrameNumber);
  CaptureFrame.HasPendingReadback = false;
  if (m_FrameOutput == nullptr || !m_CapturedFrame.has_value()) {
    return;
  }

  const CapturedFrame &Captured = *m_CapturedFrame;
  const auto *Bytes =
      reinterpret_cast<const std::byte *>(Captured.Pixels.data());
  m_FrameOutput->OnViewportFrame({
      .FrameIndex = Captured.FrameIndex,
      .Width = Captured.Width,
      .Height = Captured.Height,
      .Format = ViewportFrameFormat::R8G8B8A8Unorm,
      .Pixels = std::span<const std::byte>(Bytes, Captured.Pixels.size()),
      .User = CaptureFrame.SubmittedUser,
  });
}
} // namespace Axiom
