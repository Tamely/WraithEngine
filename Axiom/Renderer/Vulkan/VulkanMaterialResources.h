#pragma once

#include "Renderer/Material.h"
#include "Renderer/Vulkan/VulkanTypes.h"

#include <functional>
#include <unordered_map>

namespace Axiom {
struct DeletionQueue;

class VulkanMaterialResources {
public:
  struct CreateInfo {
    VmaAllocator Allocator{nullptr};
    VkDevice Device{VK_NULL_HANDLE};
    VkQueue GraphicsQueue{VK_NULL_HANDLE};
    DeletionQueue *DeletionQueue{nullptr};
    std::function<void(std::function<void(VkCommandBuffer cmd)> &&)>
        ImmediateSubmit;
  };

  void Init(const CreateInfo &CreateInfo);
  void Shutdown();

  void InitFallbackTexture();
  VkImageView ResolveMaterialTextureView(const MaterialInstanceRef &Material);
  VkImageView GetFallbackTextureView() const { return m_FallbackTexture.ImageView; }

private:
  AllocatedImage CreateTextureImage(const TextureSourceData &TextureData);

  VmaAllocator m_Allocator{nullptr};
  VkDevice m_Device{VK_NULL_HANDLE};
  VkQueue m_GraphicsQueue{VK_NULL_HANDLE};
  DeletionQueue *m_DeletionQueue{nullptr};
  std::function<void(std::function<void(VkCommandBuffer cmd)> &&)>
      m_ImmediateSubmit;
  AllocatedImage m_FallbackTexture;
  std::unordered_map<const MaterialInstance *, VkImageView> m_MaterialImageViews;
};
} // namespace Axiom
