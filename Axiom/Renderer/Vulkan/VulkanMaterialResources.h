#pragma once

#include "Renderer/Material.h"
#include "Renderer/Vulkan/VulkanTypes.h"

#include <functional>
#include <unordered_map>

namespace Axiom {
class VulkanMaterialResources {
public:
  struct CreateInfo {
    std::function<AllocatedImage(const TextureSourceData &)> CreateTextureImage;
  };

  void Init(const CreateInfo &CreateInfo);
  void Shutdown();

  void InitFallbackTexture();
  VkImageView ResolveMaterialTextureView(const MaterialInstanceRef &Material);
  VkImageView GetFallbackTextureView() const { return m_FallbackTexture.ImageView; }

private:
  std::function<AllocatedImage(const TextureSourceData &)> m_CreateTextureImage;
  AllocatedImage m_FallbackTexture;
  std::unordered_map<const MaterialInstance *, VkImageView> m_MaterialImageViews;
};
} // namespace Axiom
