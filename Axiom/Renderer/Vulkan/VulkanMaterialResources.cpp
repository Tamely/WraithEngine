#include "Renderer/Vulkan/VulkanMaterialResources.h"

#include <array>

namespace Axiom {
void VulkanMaterialResources::Init(const CreateInfo &CreateInfo) {
  m_CreateTextureImage = CreateInfo.CreateTextureImage;
}

void VulkanMaterialResources::Shutdown() {
  m_MaterialImageViews.clear();
  m_FallbackTexture = {};
}

void VulkanMaterialResources::InitFallbackTexture() {
  TextureSourceData CheckerTexture{};
  constexpr uint32_t TextureSize = 64;
  constexpr uint32_t CellSize = 8;
  constexpr std::array<std::uint8_t, 4> Purple = {0xA0, 0x20, 0xF0, 0xFF};
  constexpr std::array<std::uint8_t, 4> Black = {0x00, 0x00, 0x00, 0xFF};

  CheckerTexture.Width = TextureSize;
  CheckerTexture.Height = TextureSize;
  CheckerTexture.Pixels.resize(TextureSize * TextureSize * 4);

  for (uint32_t Y = 0; Y < TextureSize; ++Y) {
    for (uint32_t X = 0; X < TextureSize; ++X) {
      const bool UsePurple = ((X / CellSize) + (Y / CellSize)) % 2 == 0;
      const auto &Color = UsePurple ? Purple : Black;
      const size_t PixelIndex =
          (static_cast<size_t>(Y) * TextureSize + X) * 4;
      CheckerTexture.Pixels[PixelIndex + 0] = Color[0];
      CheckerTexture.Pixels[PixelIndex + 1] = Color[1];
      CheckerTexture.Pixels[PixelIndex + 2] = Color[2];
      CheckerTexture.Pixels[PixelIndex + 3] = Color[3];
    }
  }

  m_FallbackTexture = m_CreateTextureImage(CheckerTexture);
}

VkImageView
VulkanMaterialResources::ResolveMaterialTextureView(const MaterialInstanceRef &Material) {
  if (!Material || !Material->BaseColorTexture ||
      !Material->BaseColorTexture->IsValid()) {
    return m_FallbackTexture.ImageView;
  }

  auto It = m_MaterialImageViews.find(Material.get());
  if (It != m_MaterialImageViews.end()) {
    return It->second;
  }

  const AllocatedImage TextureImage =
      m_CreateTextureImage(*Material->BaseColorTexture);
  m_MaterialImageViews.emplace(Material.get(), TextureImage.ImageView);
  return TextureImage.ImageView;
}
} // namespace Axiom
