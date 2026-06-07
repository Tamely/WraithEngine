#pragma once

#include "Renderer/Material.h"
#include "Renderer/RendererTypes.h"
#include "Renderer/RenderSurface.h"
#include "AxiomRHI/Vulkan/VulkanCommandContext.h"
#include "AxiomRHI/Vulkan/VulkanDescriptors.h"
#include "AxiomRHI/Vulkan/VulkanDevice.h"
#include "AxiomRHI/Vulkan/VulkanRendererTypes.h"
#include "AxiomRHI/Vulkan/VulkanSwapchain.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Axiom {
class VulkanContext;

class VulkanResourceManager {
public:
  struct OffscreenCaptureFrame {
    AllocatedBuffer ReadbackBuffer;
    bool HasPendingReadback{false};
    uint64_t SubmittedFrameNumber{0};
    SessionUserId SubmittedUser{};
  };

  struct CreateInfo {
    VulkanContext &Context;
    VulkanDevice &Device;
    VkExtent2D WindowExtent{};
    bool HasPresentationSurface{false};
    RendererAttachmentRequirements AttachmentRequirements{};
    std::function<void(std::function<void(VkCommandBuffer)> &&,
                       std::function<void()> &&)>
        SubmitTransferUpload;
  };

  void Init(const CreateInfo &CreateInfo);
  void Shutdown();

  AllocatedImage CreateManagedTextureImage(const TextureSourceData &TextureData);
  AllocatedImage CreateManagedTextureImage(const HDRTextureSourceData &TextureData);
  void SyncHDRSkyboxTexture(HDRTextureSourceDataRef Wanted,
                            FrameData &CurrentFrame);

  VulkanSwapchain &GetSwapchain() { return m_Swapchain; }
  const VulkanSwapchain &GetSwapchain() const { return m_Swapchain; }

  DescriptorAllocator &GetDescriptorAllocator() { return m_GlobalDescriptorAllocator; }
  const DescriptorAllocator &GetDescriptorAllocator() const {
    return m_GlobalDescriptorAllocator;
  }

  VkDescriptorSet GetDrawImageDescriptorSet() const { return m_DrawImageDescriptorSet; }
  VkDescriptorSetLayout GetDrawImageDescriptorLayout() const {
    return m_DrawImageDescriptorLayout;
  }
  VkDescriptorSetLayout GetHzbReduceDescriptorLayout() const {
    return m_HzbReduceDescriptorLayout;
  }
  VkDescriptorSetLayout GetMeshGraphicsFrameDescriptorLayout() const {
    return m_MeshGraphicsFrameDescriptorLayout;
  }
  VkDescriptorSetLayout GetMeshGraphicsMaterialDescriptorLayout() const {
    return m_MeshGraphicsMaterialDescriptorLayout;
  }
  VkDescriptorSetLayout GetMeshComputeFrameDescriptorLayout() const {
    return m_MeshComputeFrameDescriptorLayout;
  }
  VkDescriptorSetLayout GetMeshDescriptorLayout() const {
    return m_MeshDescriptorLayout;
  }

  VkSampler GetLinearDepthSampler() const { return m_LinearDepthSampler; }
  VkSampler GetTextureSampler() const { return m_TextureSampler; }
  VkSampler GetHDRSkyboxSampler() const { return m_HDRSkyboxSampler; }
  VkDescriptorSetLayout GetHDRSkyboxDescriptorLayout() const {
    return m_HDRSkyboxDescriptorLayout;
  }
  VkDescriptorSet GetHDRSkyboxDescriptorSet() const { return m_HDRSkyboxDescriptorSet; }
  bool HasHDRSkyboxTexture() const { return m_LoadedHDRSkyboxData != nullptr; }

  AllocatedImage &GetDrawImage() { return m_DrawImage; }
  const AllocatedImage &GetDrawImage() const { return m_DrawImage; }
  AllocatedImage &GetDepthImage() { return m_DepthImage; }
  const AllocatedImage &GetDepthImage() const { return m_DepthImage; }
  AllocatedImage &GetRasterDepthImage() { return m_RasterDepthImage; }
  const AllocatedImage &GetRasterDepthImage() const { return m_RasterDepthImage; }
  AllocatedImage &GetHzbImage() { return m_HzbImage; }
  const AllocatedImage &GetHzbImage() const { return m_HzbImage; }

  VkImageLayout GetHzbImageLayout() const { return m_HzbImageLayout; }
  void SetHzbImageLayout(VkImageLayout Layout) { m_HzbImageLayout = Layout; }

  const std::vector<VkImageView> &GetHzbMipImageViews() const {
    return m_HzbMipImageViews;
  }
  const std::vector<VkDescriptorSet> &GetHzbReduceDescriptorSets() const {
    return m_HzbReduceDescriptorSets;
  }
  const std::vector<VkExtent2D> &GetHzbMipExtents() const { return m_HzbMipExtents; }
  const std::vector<VkDeviceSize> &GetHzbMipOffsets() const { return m_HzbMipOffsets; }
  VkDeviceSize GetHzbReadbackBufferSize() const { return m_HzbReadbackBufferSize; }

  std::array<MeshFrameResources, FRAME_OVERLAP> &GetMeshFrames() {
    return m_MeshFrames;
  }
  const std::array<MeshFrameResources, FRAME_OVERLAP> &GetMeshFrames() const {
    return m_MeshFrames;
  }
  MeshFrameResources &GetMeshFrame(uint64_t FrameNumber) {
    return m_MeshFrames[FrameNumber % FRAME_OVERLAP];
  }
  void EnsureOpaqueIndirectCapacity(MeshFrameResources &Frame,
                                    size_t DrawCapacity);

  std::array<OffscreenCaptureFrame, FRAME_OVERLAP> &GetOffscreenCaptureFrames() {
    return m_OffscreenCaptureFrames;
  }
  OffscreenCaptureFrame &GetOffscreenCaptureFrame(uint64_t FrameNumber) {
    return m_OffscreenCaptureFrames[FrameNumber % FRAME_OVERLAP];
  }

private:
  void InitSwapchain();
  void InitViewportReadbackBuffers();
  void InitHzbResources();
  void InitDescriptors();
  void InitMeshFrameResources();
  void UpdateGraphicsFrameDescriptor(const MeshFrameResources &Frame) const;
  AllocatedImage CreateTextureImage(const TextureSourceData &TextureData,
                                    bool TrackForShutdown);
  AllocatedImage CreateTextureImage(const HDRTextureSourceData &TextureData,
                                    bool TrackForShutdown);
  void DestroyHDRSkyboxTexture();
  void DestroyManagedImage(AllocatedImage &Image);
  void ConfigureTransferSharing(VkImageCreateInfo &ImageInfo) const;
  void ConfigureTransferSharing(VkBufferCreateInfo &BufferInfo) const;

private:
  VulkanContext *m_Context{nullptr};
  VulkanDevice *m_Device{nullptr};
  VkExtent2D m_WindowExtent{1700, 900};
  bool m_HasPresentationSurface{false};
  RendererAttachmentRequirements m_AttachmentRequirements{};
  std::function<void(std::function<void(VkCommandBuffer)> &&,
                     std::function<void()> &&)>
      m_SubmitTransferUpload;

  VulkanSwapchain m_Swapchain;
  DescriptorAllocator m_GlobalDescriptorAllocator;
  VkDescriptorSet m_DrawImageDescriptorSet{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_DrawImageDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_HzbReduceDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshGraphicsFrameDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshGraphicsMaterialDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshComputeFrameDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshDescriptorLayout{VK_NULL_HANDLE};
  VkSampler m_LinearDepthSampler{VK_NULL_HANDLE};
  VkSampler m_TextureSampler{VK_NULL_HANDLE};

  VkDescriptorSetLayout m_HDRSkyboxDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSet m_HDRSkyboxDescriptorSet{VK_NULL_HANDLE};
  VkSampler m_HDRSkyboxSampler{VK_NULL_HANDLE};
  AllocatedImage m_HDRSkyboxImage{};
  HDRTextureSourceDataRef m_LoadedHDRSkyboxData{nullptr};

  AllocatedImage m_DrawImage;
  AllocatedImage m_DepthImage;
  AllocatedImage m_RasterDepthImage;
  AllocatedImage m_HzbImage;
  std::array<OffscreenCaptureFrame, FRAME_OVERLAP> m_OffscreenCaptureFrames{};
  std::vector<VkImageView> m_HzbMipImageViews;
  std::vector<VkDescriptorSet> m_HzbReduceDescriptorSets;
  std::vector<VkExtent2D> m_HzbMipExtents;
  std::vector<VkDeviceSize> m_HzbMipOffsets;
  VkDeviceSize m_HzbReadbackBufferSize{0};
  VkImageLayout m_HzbImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};
  std::array<MeshFrameResources, FRAME_OVERLAP> m_MeshFrames{};
  std::vector<AllocatedImage> m_ManagedTextureImages;
};
} // namespace Axiom
