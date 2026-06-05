#pragma once

#include "Renderer/Material.h"
#include "AxiomRHI/Vulkan/VulkanDescriptors.h"
#include "AxiomRHI/Vulkan/VulkanTypes.h"

#include <functional>
#include <memory>
#include <unordered_map>

namespace Axiom {
class VulkanMaterialResources {
public:
  struct CreateInfo {
    VkDevice Device{VK_NULL_HANDLE};
    DescriptorAllocator *DescriptorAllocator{nullptr};
    VkDescriptorSetLayout MaterialDescriptorSetLayout{VK_NULL_HANDLE};
    VkSampler TextureSampler{VK_NULL_HANDLE};
    std::function<AllocatedImage(const TextureSourceData &)> CreateTextureImage;
  };

  void Init(const CreateInfo &CreateInfo);
  void Shutdown();

  void InitFallbackTexture();
  MaterialHandle CreateMaterialHandle(const MaterialInstance &Material);
  void UpdateMaterialHandle(MaterialHandle Handle, const MaterialInstance &Material);
  const MaterialInstance *ResolveMaterialHandle(MaterialHandle Handle) const;
  VkImageView ResolveMaterialTextureView(const MaterialInstance *Material);
  VkDescriptorSet ResolveMaterialDescriptorSet(const MaterialInstance *Material);
  VkImageView GetFallbackTextureView() const { return m_FallbackTexture.ImageView; }
#if !defined(NDEBUG)
  void ResetDebugCounters();
  uint32_t GetDebugGraphicsMaterialDescriptorUpdates() const {
    return m_DebugGraphicsMaterialDescriptorUpdates;
  }
#endif

private:
  struct MaterialDescriptorCacheEntry {
    VkDescriptorSet DescriptorSet{VK_NULL_HANDLE};
    VkImageView TextureView{VK_NULL_HANDLE};
    const TextureSourceData *TextureSource{nullptr};
    uint64_t Revision{0};
  };

  VkDevice m_Device{VK_NULL_HANDLE};
  DescriptorAllocator *m_DescriptorAllocator{nullptr};
  VkDescriptorSetLayout m_MaterialDescriptorSetLayout{VK_NULL_HANDLE};
  VkSampler m_TextureSampler{VK_NULL_HANDLE};
  std::function<AllocatedImage(const TextureSourceData &)> m_CreateTextureImage;
  AllocatedImage m_FallbackTexture;
  std::unordered_map<MaterialHandle, std::unique_ptr<MaterialInstance>,
                     MaterialHandleHash>
      m_MaterialsByHandle;
  std::unordered_map<const MaterialInstance *, VkImageView> m_MaterialImageViews;
  std::unordered_map<const MaterialInstance *, MaterialDescriptorCacheEntry>
      m_MaterialDescriptorSets;
  uint32_t m_NextMaterialHandleValue{1};
#if !defined(NDEBUG)
  uint32_t m_DebugGraphicsMaterialDescriptorUpdates{0};
#endif
};
} // namespace Axiom
