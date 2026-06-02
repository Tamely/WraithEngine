#pragma once

#include "Renderer/RenderScene.h"
#include "Renderer/Vulkan/VulkanDeletionQueue.h"
#include "Renderer/Vulkan/VulkanDescriptors.h"
#include "Renderer/Vulkan/VulkanTypes.h"

namespace Axiom {

class VulkanLightBillboardRenderer {
public:
  struct InitInfo {
    VkDevice Device{VK_NULL_HANDLE};
    VkFormat DrawImageFormat{VK_FORMAT_UNDEFINED};
    DescriptorAllocator *DescriptorAllocator{nullptr};
    VkImageView TextureView{VK_NULL_HANDLE};
    VkSampler TextureSampler{VK_NULL_HANDLE};
  };

  void Init(const InitInfo &Info, DeletionQueue &DeletionQueue);

  void DrawLightBillboards(VkCommandBuffer CommandBuffer, VkExtent2D DrawExtent,
                           VkImageView DrawImageView, const RenderScene &Scene);

  bool IsInitialized() const { return m_Pipeline != VK_NULL_HANDLE; }

private:
  VkDevice m_Device{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_DescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSet m_DescriptorSet{VK_NULL_HANDLE};
  VkPipeline m_Pipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_PipelineLayout{VK_NULL_HANDLE};
};

} // namespace Axiom
