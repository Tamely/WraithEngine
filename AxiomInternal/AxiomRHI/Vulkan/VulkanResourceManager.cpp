#include "AxiomRHI/Vulkan/VulkanResourceManager.h"

#include "AxiomRHI/Vulkan/VulkanBuffer.h"
#include "AxiomRHI/Vulkan/VulkanImage.h"
#include "AxiomRHI/Vulkan/VulkanInitializers.h"

#include <array>
#include <cstring>

namespace Axiom {
namespace {
constexpr uint32_t MaxGraphicsMaterialTextures = 1024;

uint32_t ComputeHzbMipCount(VkExtent2D BaseExtent) {
  uint32_t Width = BaseExtent.width;
  uint32_t Height = BaseExtent.height;
  uint32_t MipCount = 0;
  while (Width > 0 && Height > 0) {
    ++MipCount;
    Width = std::max(1u, Width / 2u);
    Height = std::max(1u, Height / 2u);
    if (Width == 1u && Height == 1u) {
      ++MipCount;
      break;
    }
  }
  return std::max(1u, MipCount - 1u);
}

VkExtent2D ComputeHzbMipExtent(VkExtent2D BaseExtent, uint32_t MipLevel) {
  return {
      std::max(1u, BaseExtent.width >> MipLevel),
      std::max(1u, BaseExtent.height >> MipLevel),
  };
}

void PopulateTextureImage(VkCommandBuffer CommandBuffer, const AllocatedImage &Image,
                          VkBuffer StagingBuffer) {
  VkUtil::TransitionImage(CommandBuffer, Image.Image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkBufferImageCopy Region{};
  Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  Region.imageSubresource.layerCount = 1;
  Region.imageExtent = Image.ImageExtent;
  vkCmdCopyBufferToImage(CommandBuffer, StagingBuffer, Image.Image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

  VkUtil::TransitionImage(CommandBuffer, Image.Image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
} // namespace

void VulkanResourceManager::Init(const CreateInfo &CreateInfo) {
  m_Context = &CreateInfo.Context;
  m_Device = &CreateInfo.Device;
  m_WindowExtent = CreateInfo.WindowExtent;
  m_HasPresentationSurface = CreateInfo.HasPresentationSurface;
  m_AttachmentRequirements = CreateInfo.AttachmentRequirements;
  m_SubmitTransferUpload = CreateInfo.SubmitTransferUpload;

  InitSwapchain();
  InitHzbResources();
  InitViewportReadbackBuffers();
  InitDescriptors();
  InitMeshFrameResources();
}

void VulkanResourceManager::Shutdown() {
  DestroyHDRSkyboxTexture();

  for (auto &Image : m_ManagedTextureImages) {
    DestroyManagedImage(Image);
  }
  m_ManagedTextureImages.clear();

  for (auto &Frame : m_MeshFrames) {
    if (Frame.TimestampQueryPool != VK_NULL_HANDLE) {
      vkDestroyQueryPool(m_Device->Device, Frame.TimestampQueryPool,
                         VK_NULL_HANDLE);
      Frame.TimestampQueryPool = VK_NULL_HANDLE;
    }
    VkBufferUtil::DestroyBuffer(m_Device->Allocator, Frame.CameraBuffer);
    VkBufferUtil::DestroyBuffer(m_Device->Allocator, Frame.OpaqueObjectBuffer);
    VkBufferUtil::DestroyBuffer(m_Device->Allocator, Frame.OpaqueIndirectBuffer);
    VkBufferUtil::DestroyBuffer(m_Device->Allocator, Frame.HzbReadbackBuffer);
  }

  for (auto &CaptureFrame : m_OffscreenCaptureFrames) {
    VkBufferUtil::DestroyBuffer(m_Device->Allocator, CaptureFrame.ReadbackBuffer);
    CaptureFrame = {};
  }

  for (VkImageView MipView : m_HzbMipImageViews) {
    if (MipView != VK_NULL_HANDLE) {
      vkDestroyImageView(m_Device->Device, MipView, VK_NULL_HANDLE);
    }
  }
  m_HzbMipImageViews.clear();
  m_HzbMipExtents.clear();
  m_HzbMipOffsets.clear();
  DestroyManagedImage(m_HzbImage);

  if (m_HDRSkyboxSampler != VK_NULL_HANDLE) {
    vkDestroySampler(m_Device->Device, m_HDRSkyboxSampler, VK_NULL_HANDLE);
    m_HDRSkyboxSampler = VK_NULL_HANDLE;
  }
  if (m_TextureSampler != VK_NULL_HANDLE) {
    vkDestroySampler(m_Device->Device, m_TextureSampler, VK_NULL_HANDLE);
    m_TextureSampler = VK_NULL_HANDLE;
  }
  if (m_LinearDepthSampler != VK_NULL_HANDLE) {
    vkDestroySampler(m_Device->Device, m_LinearDepthSampler, VK_NULL_HANDLE);
    m_LinearDepthSampler = VK_NULL_HANDLE;
  }

  if (m_MeshDescriptorLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_Device->Device, m_MeshDescriptorLayout,
                                 VK_NULL_HANDLE);
    m_MeshDescriptorLayout = VK_NULL_HANDLE;
  }
  if (m_MeshComputeFrameDescriptorLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_Device->Device,
                                 m_MeshComputeFrameDescriptorLayout,
                                 VK_NULL_HANDLE);
    m_MeshComputeFrameDescriptorLayout = VK_NULL_HANDLE;
  }
  if (m_MeshGraphicsFrameDescriptorLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_Device->Device,
                                 m_MeshGraphicsFrameDescriptorLayout,
                                 VK_NULL_HANDLE);
    m_MeshGraphicsFrameDescriptorLayout = VK_NULL_HANDLE;
  }
  if (m_MeshGraphicsMaterialDescriptorLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_Device->Device,
                                 m_MeshGraphicsMaterialDescriptorLayout,
                                 VK_NULL_HANDLE);
    m_MeshGraphicsMaterialDescriptorLayout = VK_NULL_HANDLE;
  }
  if (m_HzbReduceDescriptorLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_Device->Device, m_HzbReduceDescriptorLayout,
                                 VK_NULL_HANDLE);
    m_HzbReduceDescriptorLayout = VK_NULL_HANDLE;
  }
  if (m_DrawImageDescriptorLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_Device->Device, m_DrawImageDescriptorLayout,
                                 VK_NULL_HANDLE);
    m_DrawImageDescriptorLayout = VK_NULL_HANDLE;
  }
  if (m_HDRSkyboxDescriptorLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_Device->Device, m_HDRSkyboxDescriptorLayout,
                                 VK_NULL_HANDLE);
    m_HDRSkyboxDescriptorLayout = VK_NULL_HANDLE;
  }
  m_GlobalDescriptorAllocator.DestroyPool(m_Device->Device);

  DestroyManagedImage(m_RasterDepthImage);
  DestroyManagedImage(m_DepthImage);
  DestroyManagedImage(m_DrawImage);

  m_Swapchain.Shutdown(*m_Device);
  m_AttachmentRequirements = {};
}

void VulkanResourceManager::InitSwapchain() {
  if (m_HasPresentationSurface) {
    m_Swapchain.Init(*m_Context, *m_Device, m_WindowExtent.width,
                     m_WindowExtent.height);
  } else {
    m_Swapchain.ImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    m_Swapchain.Extent = m_WindowExtent;
  }

  const VkExtent3D DrawImageExtent = {
      m_WindowExtent.width, m_WindowExtent.height, 1};

  if (m_AttachmentRequirements.NeedsGBuffer &&
      m_AttachmentRequirements.GBufferColorTargetCount > 0) {
    // Deferred attachments are not implemented in this PR; forward continues to
    // allocate its single HDR target plus depth resources.
  }

  m_DrawImage.ImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
  m_DrawImage.ImageExtent = DrawImageExtent;

  VkImageCreateInfo DrawInfo = VkInit::ImageCreateInfo(
      m_DrawImage.ImageFormat,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      m_DrawImage.ImageExtent);

  VmaAllocationCreateInfo DrawAllocInfo{};
  DrawAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  DrawAllocInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VK_CHECK(vmaCreateImage(m_Device->Allocator, &DrawInfo, &DrawAllocInfo,
                          &m_DrawImage.Image, &m_DrawImage.Allocation,
                          VK_NULL_HANDLE));
  VkImageViewCreateInfo DrawViewInfo = VkInit::ImageViewCreateInfo(
      m_DrawImage.ImageFormat, m_DrawImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device->Device, &DrawViewInfo, VK_NULL_HANDLE,
                             &m_DrawImage.ImageView));

  m_DepthImage.ImageFormat = VK_FORMAT_R32_UINT;
  m_DepthImage.ImageExtent = DrawImageExtent;
  VkImageCreateInfo DepthInfo = VkInit::ImageCreateInfo(
      m_DepthImage.ImageFormat,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
      m_DepthImage.ImageExtent);
  VK_CHECK(vmaCreateImage(m_Device->Allocator, &DepthInfo, &DrawAllocInfo,
                          &m_DepthImage.Image, &m_DepthImage.Allocation,
                          VK_NULL_HANDLE));
  VkImageViewCreateInfo DepthViewInfo = VkInit::ImageViewCreateInfo(
      m_DepthImage.ImageFormat, m_DepthImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device->Device, &DepthViewInfo, VK_NULL_HANDLE,
                             &m_DepthImage.ImageView));

  m_RasterDepthImage.ImageFormat = VK_FORMAT_D32_SFLOAT;
  m_RasterDepthImage.ImageExtent = DrawImageExtent;
  VkImageCreateInfo RasterDepthInfo = VkInit::ImageCreateInfo(
      m_RasterDepthImage.ImageFormat,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      m_RasterDepthImage.ImageExtent);
  VK_CHECK(vmaCreateImage(m_Device->Allocator, &RasterDepthInfo, &DrawAllocInfo,
                          &m_RasterDepthImage.Image,
                          &m_RasterDepthImage.Allocation, VK_NULL_HANDLE));
  VkImageViewCreateInfo RasterDepthViewInfo = VkInit::ImageViewCreateInfo(
      m_RasterDepthImage.ImageFormat, m_RasterDepthImage.Image,
      VK_IMAGE_ASPECT_DEPTH_BIT);
  VK_CHECK(vkCreateImageView(m_Device->Device, &RasterDepthViewInfo,
                             VK_NULL_HANDLE, &m_RasterDepthImage.ImageView));
}

void VulkanResourceManager::InitViewportReadbackBuffers() {
  const size_t BufferSize =
      static_cast<size_t>(m_DrawImage.ImageExtent.width) *
      static_cast<size_t>(m_DrawImage.ImageExtent.height) * sizeof(uint16_t) *
      4u;
  for (auto &CaptureFrame : m_OffscreenCaptureFrames) {
    CaptureFrame.ReadbackBuffer = VkBufferUtil::CreateBuffer(
        m_Device->Allocator, BufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_TO_CPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);
  }
}

void VulkanResourceManager::InitHzbResources() {
  const VkExtent2D BaseExtent = {m_DrawImage.ImageExtent.width,
                                 m_DrawImage.ImageExtent.height};
  const uint32_t MipCount = ComputeHzbMipCount(BaseExtent);

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
  VK_CHECK(vmaCreateImage(m_Device->Allocator, &HzbInfo, &AllocationInfo,
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
    VK_CHECK(vkCreateImageView(m_Device->Device, &ViewInfo, VK_NULL_HANDLE,
                               &MipView));
    m_HzbMipImageViews.push_back(MipView);
  }
}

void VulkanResourceManager::InitDescriptors() {
  const uint32_t InitialSetCount =
      5 + (FRAME_OVERLAP * 3) +
      static_cast<uint32_t>(m_HzbMipImageViews.size()) + 64;
  std::vector<DescriptorAllocator::PoolSizeRatio> Sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4.0f},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6.0f},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 20.0f},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 2.0f},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4.0f},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.0f}};
  m_GlobalDescriptorAllocator.InitPool(m_Device->Device, InitialSetCount, Sizes);

  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_DrawImageDescriptorLayout =
        Builder.Build(m_Device->Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }
  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_HzbReduceDescriptorLayout =
        Builder.Build(m_Device->Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }
  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    m_MeshGraphicsFrameDescriptorLayout =
        Builder.Build(m_Device->Device,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }
  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                       MaxGraphicsMaterialTextures);
    Builder.AddBinding(2, VK_DESCRIPTOR_TYPE_SAMPLER);
    m_MeshGraphicsMaterialDescriptorLayout =
        Builder.Build(m_Device->Device,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }
  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Builder.AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_MeshComputeFrameDescriptorLayout =
        Builder.Build(m_Device->Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }
  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    Builder.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    m_MeshDescriptorLayout =
        Builder.Build(m_Device->Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }
  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_HDRSkyboxDescriptorLayout =
        Builder.Build(m_Device->Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  m_DrawImageDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
      m_Device->Device, m_DrawImageDescriptorLayout);
  VkDescriptorImageInfo DrawImageInfo{};
  DrawImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  DrawImageInfo.imageView = m_DrawImage.ImageView;
  const VkWriteDescriptorSet DrawImageWrite = VkInit::WriteDescriptorSet(
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_DrawImageDescriptorSet, &DrawImageInfo,
      0);
  vkUpdateDescriptorSets(m_Device->Device, 1, &DrawImageWrite, 0,
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
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
      .unnormalizedCoordinates = VK_FALSE};
  VK_CHECK(vkCreateSampler(m_Device->Device, &SamplerInfo, VK_NULL_HANDLE,
                           &m_LinearDepthSampler));

  SamplerInfo.magFilter = VK_FILTER_LINEAR;
  SamplerInfo.minFilter = VK_FILTER_LINEAR;
  SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  SamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  SamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  SamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VK_CHECK(vkCreateSampler(m_Device->Device, &SamplerInfo, VK_NULL_HANDLE,
                           &m_TextureSampler));

  VkSamplerCreateInfo HDRSamplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE};
  VK_CHECK(vkCreateSampler(m_Device->Device, &HDRSamplerInfo, VK_NULL_HANDLE,
                           &m_HDRSkyboxSampler));

  m_HzbReduceDescriptorSets.reserve(m_HzbMipImageViews.size());
  for (size_t MipLevel = 0; MipLevel < m_HzbMipImageViews.size(); ++MipLevel) {
    VkDescriptorSet DescriptorSet = m_GlobalDescriptorAllocator.Allocate(
        m_Device->Device, m_HzbReduceDescriptorLayout);
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

    const std::array<VkWriteDescriptorSet, 2> Writes = {
        VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   DescriptorSet, &DestinationImageInfo, 0),
        VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   DescriptorSet, &SourceImageInfo, 1)};
    vkUpdateDescriptorSets(m_Device->Device, static_cast<uint32_t>(Writes.size()),
                           Writes.data(), 0, VK_NULL_HANDLE);
  }
}

void VulkanResourceManager::InitMeshFrameResources() {
  for (auto &Frame : m_MeshFrames) {
    Frame.CameraBuffer = VkBufferUtil::CreateBuffer(
        m_Device->Allocator, sizeof(CameraFrameUniform),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);
    Frame.OpaqueObjectBuffer = VkBufferUtil::CreateBuffer(
        m_Device->Allocator, sizeof(MeshGraphicsObjectData),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);
    Frame.OpaqueIndirectBuffer = VkBufferUtil::CreateBuffer(
        m_Device->Allocator, sizeof(VkDrawIndexedIndirectCommand),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);
    Frame.OpaqueObjectCapacity = 1;
    Frame.OpaqueIndirectCapacity = 1;
    Frame.HzbReadbackBuffer = VkBufferUtil::CreateBuffer(
        m_Device->Allocator, static_cast<size_t>(m_HzbReadbackBufferSize),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);

    Frame.DepthFrameDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
        m_Device->Device, m_MeshGraphicsFrameDescriptorLayout);
    Frame.GraphicsFrameDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
        m_Device->Device, m_MeshGraphicsFrameDescriptorLayout);
    Frame.ComputeFrameDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
        m_Device->Device, m_MeshComputeFrameDescriptorLayout);

    VkQueryPoolCreateInfo QueryPoolInfo{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = TimestampQueryCount};
    VK_CHECK(vkCreateQueryPool(m_Device->Device, &QueryPoolInfo, VK_NULL_HANDLE,
                               &Frame.TimestampQueryPool));
    UpdateGraphicsFrameDescriptor(Frame);
  }
}

void VulkanResourceManager::EnsureOpaqueIndirectCapacity(
    MeshFrameResources &Frame, size_t DrawCapacity) {
  const size_t WantedCapacity = std::max<size_t>(1, DrawCapacity);
  if (Frame.OpaqueObjectCapacity >= WantedCapacity &&
      Frame.OpaqueIndirectCapacity >= WantedCapacity) {
    return;
  }

  size_t NewCapacity =
      std::max(Frame.OpaqueObjectCapacity, Frame.OpaqueIndirectCapacity);
  NewCapacity = std::max<size_t>(1, NewCapacity);
  while (NewCapacity < WantedCapacity) {
    NewCapacity *= 2;
  }

  VkBufferUtil::DestroyBuffer(m_Device->Allocator, Frame.OpaqueObjectBuffer);
  VkBufferUtil::DestroyBuffer(m_Device->Allocator, Frame.OpaqueIndirectBuffer);
  Frame.OpaqueObjectBuffer = VkBufferUtil::CreateBuffer(
      m_Device->Allocator, NewCapacity * sizeof(MeshGraphicsObjectData),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT);
  Frame.OpaqueIndirectBuffer = VkBufferUtil::CreateBuffer(
      m_Device->Allocator, NewCapacity * sizeof(VkDrawIndexedIndirectCommand),
      VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT);
  Frame.OpaqueObjectCapacity = NewCapacity;
  Frame.OpaqueIndirectCapacity = NewCapacity;
  UpdateGraphicsFrameDescriptor(Frame);
}

void VulkanResourceManager::UpdateGraphicsFrameDescriptor(
    const MeshFrameResources &Frame) const {
  VkDescriptorBufferInfo CameraBufferInfo =
      VkInit::BufferInfo(Frame.CameraBuffer.Buffer, 0, Frame.CameraBuffer.Size);
  VkDescriptorBufferInfo ObjectBufferInfo =
      VkInit::BufferInfo(Frame.OpaqueObjectBuffer.Buffer, 0,
                         Frame.OpaqueObjectBuffer.Size);
  const std::array<VkWriteDescriptorSet, 2> Writes = {
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    Frame.GraphicsFrameDescriptorSet,
                                    &CameraBufferInfo, 0),
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    Frame.GraphicsFrameDescriptorSet,
                                    &ObjectBufferInfo, 1)};
  vkUpdateDescriptorSets(m_Device->Device, static_cast<uint32_t>(Writes.size()),
                         Writes.data(), 0, VK_NULL_HANDLE);
}

AllocatedImage
VulkanResourceManager::CreateManagedTextureImage(const TextureSourceData &TextureData) {
  return CreateTextureImage(TextureData, true);
}

AllocatedImage VulkanResourceManager::CreateManagedTextureImage(
    const HDRTextureSourceData &TextureData) {
  return CreateTextureImage(TextureData, true);
}

AllocatedImage VulkanResourceManager::CreateTextureImage(
    const TextureSourceData &TextureData, bool TrackForShutdown) {
  AllocatedImage TextureImage{};
  TextureImage.ImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
  TextureImage.ImageExtent = {TextureData.Width, TextureData.Height, 1};

  VkImageCreateInfo ImageInfo = VkInit::ImageCreateInfo(
      TextureImage.ImageFormat,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      TextureImage.ImageExtent);
  ConfigureTransferSharing(ImageInfo);

  VmaAllocationCreateInfo AllocationInfo{};
  AllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  AllocationInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK(vmaCreateImage(m_Device->Allocator, &ImageInfo, &AllocationInfo,
                          &TextureImage.Image, &TextureImage.Allocation,
                          VK_NULL_HANDLE));

  VkImageViewCreateInfo ViewInfo = VkInit::ImageViewCreateInfo(
      TextureImage.ImageFormat, TextureImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device->Device, &ViewInfo, VK_NULL_HANDLE,
                             &TextureImage.ImageView));

  const VkDeviceSize ByteCount = TextureData.Pixels.size();
  auto StagingBuffer = VkBufferUtil::CreateBuffer(
      m_Device->Allocator, static_cast<size_t>(ByteCount),
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT);
  std::memcpy(StagingBuffer.Info.pMappedData, TextureData.Pixels.data(),
              TextureData.Pixels.size());

  m_SubmitTransferUpload(
      [TextureImage, StagingBuffer](VkCommandBuffer CommandBuffer) mutable {
        PopulateTextureImage(CommandBuffer, TextureImage, StagingBuffer.Buffer);
      },
      [this, StagingBuffer]() mutable {
        VkBufferUtil::DestroyBuffer(m_Device->Allocator, StagingBuffer);
      });

  if (TrackForShutdown) {
    m_ManagedTextureImages.push_back(TextureImage);
  }
  return TextureImage;
}

AllocatedImage VulkanResourceManager::CreateTextureImage(
    const HDRTextureSourceData &TextureData, bool TrackForShutdown) {
  AllocatedImage TextureImage{};
  TextureImage.ImageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
  TextureImage.ImageExtent = {TextureData.Width, TextureData.Height, 1};

  VkImageCreateInfo ImageInfo = VkInit::ImageCreateInfo(
      TextureImage.ImageFormat,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      TextureImage.ImageExtent);
  ConfigureTransferSharing(ImageInfo);

  VmaAllocationCreateInfo AllocationInfo{};
  AllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  AllocationInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK(vmaCreateImage(m_Device->Allocator, &ImageInfo, &AllocationInfo,
                          &TextureImage.Image, &TextureImage.Allocation,
                          VK_NULL_HANDLE));

  VkImageViewCreateInfo ViewInfo = VkInit::ImageViewCreateInfo(
      TextureImage.ImageFormat, TextureImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device->Device, &ViewInfo, VK_NULL_HANDLE,
                             &TextureImage.ImageView));

  const VkDeviceSize ByteCount =
      static_cast<VkDeviceSize>(TextureData.Pixels.size()) * sizeof(float);
  auto StagingBuffer = VkBufferUtil::CreateBuffer(
      m_Device->Allocator, static_cast<size_t>(ByteCount),
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT);
  std::memcpy(StagingBuffer.Info.pMappedData, TextureData.Pixels.data(),
              static_cast<size_t>(ByteCount));

  m_SubmitTransferUpload(
      [TextureImage, StagingBuffer](VkCommandBuffer CommandBuffer) mutable {
        PopulateTextureImage(CommandBuffer, TextureImage, StagingBuffer.Buffer);
      },
      [this, StagingBuffer]() mutable {
        VkBufferUtil::DestroyBuffer(m_Device->Allocator, StagingBuffer);
      });

  if (TrackForShutdown) {
    m_ManagedTextureImages.push_back(TextureImage);
  }
  return TextureImage;
}

void VulkanResourceManager::SyncHDRSkyboxTexture(HDRTextureSourceDataRef Wanted,
                                                 FrameData &CurrentFrame) {
  if (Wanted == m_LoadedHDRSkyboxData) {
    return;
  }

  if (m_HDRSkyboxImage.Image != VK_NULL_HANDLE) {
    AllocatedImage OldImage = m_HDRSkyboxImage;
    CurrentFrame.DeletionQueue.PushFunction([this, OldImage]() mutable {
      AllocatedImage ImageCopy = OldImage;
      DestroyManagedImage(ImageCopy);
    });
    m_HDRSkyboxImage = {};
    m_HDRSkyboxDescriptorSet = VK_NULL_HANDLE;
  }

  if (Wanted && Wanted->IsValid()) {
    m_HDRSkyboxImage = CreateTextureImage(*Wanted, false);
    m_HDRSkyboxDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
        m_Device->Device, m_HDRSkyboxDescriptorLayout);

    VkDescriptorImageInfo SamplerImage{};
    SamplerImage.sampler = m_HDRSkyboxSampler;
    SamplerImage.imageView = m_HDRSkyboxImage.ImageView;
    SamplerImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    const VkWriteDescriptorSet Write = VkInit::WriteDescriptorSet(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_HDRSkyboxDescriptorSet,
        &SamplerImage, 0);
    vkUpdateDescriptorSets(m_Device->Device, 1, &Write, 0, VK_NULL_HANDLE);
  }

  m_LoadedHDRSkyboxData = Wanted;
}

void VulkanResourceManager::DestroyHDRSkyboxTexture() {
  DestroyManagedImage(m_HDRSkyboxImage);
  m_HDRSkyboxDescriptorSet = VK_NULL_HANDLE;
  m_LoadedHDRSkyboxData.reset();
}

void VulkanResourceManager::DestroyManagedImage(AllocatedImage &Image) {
  if (Image.ImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(m_Device->Device, Image.ImageView, VK_NULL_HANDLE);
    Image.ImageView = VK_NULL_HANDLE;
  }
  if (Image.Image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_Device->Allocator, Image.Image, Image.Allocation);
    Image.Image = VK_NULL_HANDLE;
    Image.Allocation = VK_NULL_HANDLE;
  }
}

void VulkanResourceManager::ConfigureTransferSharing(VkImageCreateInfo &ImageInfo) const {
  if (m_Device->TransferQueueFamily == m_Device->GraphicsQueueFamily) {
    return;
  }

  static uint32_t QueueFamilies[2];
  QueueFamilies[0] = m_Device->GraphicsQueueFamily;
  QueueFamilies[1] = m_Device->TransferQueueFamily;
  ImageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
  ImageInfo.queueFamilyIndexCount = 2;
  ImageInfo.pQueueFamilyIndices = QueueFamilies;
}

void VulkanResourceManager::ConfigureTransferSharing(
    VkBufferCreateInfo &BufferInfo) const {
  if (m_Device->TransferQueueFamily == m_Device->GraphicsQueueFamily) {
    return;
  }

  static uint32_t QueueFamilies[2];
  QueueFamilies[0] = m_Device->GraphicsQueueFamily;
  QueueFamilies[1] = m_Device->TransferQueueFamily;
  BufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
  BufferInfo.queueFamilyIndexCount = 2;
  BufferInfo.pQueueFamilyIndices = QueueFamilies;
}
} // namespace Axiom
