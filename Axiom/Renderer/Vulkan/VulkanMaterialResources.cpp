#include "Renderer/Vulkan/VulkanMaterialResources.h"

#include "Renderer/Vulkan/VulkanInitializers.h"

#include <array>
#include <cassert>

namespace Axiom {
void VulkanMaterialResources::Init(const CreateInfo &CreateInfo) {
  m_Device = CreateInfo.Device;
  m_DescriptorAllocator = CreateInfo.DescriptorAllocator;
  m_MaterialDescriptorSetLayout = CreateInfo.MaterialDescriptorSetLayout;
  m_TextureSampler = CreateInfo.TextureSampler;
  m_CreateTextureImage = CreateInfo.CreateTextureImage;
}

void VulkanMaterialResources::Shutdown() {
  m_MaterialsByHandle.clear();
  m_MaterialImageViews.clear();
  m_MaterialDescriptorSets.clear();
  m_FallbackTexture = {};
  m_NextMaterialHandleValue = 1;
#if !defined(NDEBUG)
  m_DebugGraphicsMaterialDescriptorUpdates = 0;
#endif
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

MaterialHandle
VulkanMaterialResources::CreateMaterialHandle(const MaterialInstance &Material) {
  MaterialHandle Handle{m_NextMaterialHandleValue++};
  auto [It, Inserted] = m_MaterialsByHandle.emplace(
      Handle, std::make_unique<MaterialInstance>(Material));
  assert(Inserted && "Allocated duplicate material handle");
  (void)It;
  return Handle;
}

void VulkanMaterialResources::UpdateMaterialHandle(
    MaterialHandle Handle, const MaterialInstance &Material) {
  if (!Handle.IsValid()) {
    return;
  }

  auto It = m_MaterialsByHandle.find(Handle);
  if (It == m_MaterialsByHandle.end()) {
    auto [InsertedIt, Inserted] = m_MaterialsByHandle.emplace(
        Handle, std::make_unique<MaterialInstance>(Material));
    assert(Inserted && "Failed to insert material for valid handle");
    (void)InsertedIt;
    if (Handle.Value >= m_NextMaterialHandleValue) {
      m_NextMaterialHandleValue = Handle.Value + 1;
    }
    return;
  }

  *It->second = Material;
}

const MaterialInstance *
VulkanMaterialResources::ResolveMaterialHandle(MaterialHandle Handle) const {
  if (!Handle.IsValid()) {
    return nullptr;
  }

  const auto It = m_MaterialsByHandle.find(Handle);
  return It != m_MaterialsByHandle.end() ? It->second.get() : nullptr;
}

VkImageView
VulkanMaterialResources::ResolveMaterialTextureView(const MaterialInstance *Material) {
  if (!Material || !Material->BaseColorTexture ||
      !Material->BaseColorTexture->IsValid()) {
    return m_FallbackTexture.ImageView;
  }

  auto It = m_MaterialImageViews.find(Material);
  if (It != m_MaterialImageViews.end()) {
    return It->second;
  }

  const AllocatedImage TextureImage =
      m_CreateTextureImage(*Material->BaseColorTexture);
  m_MaterialImageViews.emplace(Material, TextureImage.ImageView);
  return TextureImage.ImageView;
}

VkDescriptorSet
VulkanMaterialResources::ResolveMaterialDescriptorSet(const MaterialInstance *Material) {
  const MaterialInstance *MaterialKey = Material;
  const uint64_t MaterialRevision = Material ? Material->Revision : 0;
  const TextureSourceData *TextureSource =
      (Material && Material->BaseColorTexture &&
       Material->BaseColorTexture->IsValid())
          ? Material->BaseColorTexture.get()
          : nullptr;

  auto It = m_MaterialDescriptorSets.find(MaterialKey);
  if (It != m_MaterialDescriptorSets.end() &&
      It->second.TextureSource != TextureSource) {
    m_MaterialImageViews.erase(MaterialKey);
  }

  const VkImageView TextureView = ResolveMaterialTextureView(Material);
  if (It != m_MaterialDescriptorSets.end() &&
      It->second.Revision == MaterialRevision &&
      It->second.TextureView == TextureView) {
    return It->second.DescriptorSet;
  }

  MaterialDescriptorCacheEntry *Entry = nullptr;
  if (It == m_MaterialDescriptorSets.end()) {
    MaterialDescriptorCacheEntry NewEntry{};
    NewEntry.DescriptorSet = m_DescriptorAllocator->Allocate(
        m_Device, m_MaterialDescriptorSetLayout);
    auto [InsertedIt, Inserted] =
        m_MaterialDescriptorSets.emplace(MaterialKey, NewEntry);
    (void)Inserted;
    Entry = &InsertedIt->second;
  } else {
    Entry = &It->second;
  }

  VkDescriptorImageInfo GraphicsTextureImageInfo{};
  GraphicsTextureImageInfo.imageView = TextureView;
  GraphicsTextureImageInfo.imageLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo GraphicsTextureSamplerInfo{};
  GraphicsTextureSamplerInfo.sampler = m_TextureSampler;

  const std::array<VkWriteDescriptorSet, 2> GraphicsMaterialWrites = {
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                 Entry->DescriptorSet, &GraphicsTextureImageInfo,
                                 1),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLER,
                                 Entry->DescriptorSet,
                                 &GraphicsTextureSamplerInfo, 2)};
  vkUpdateDescriptorSets(m_Device,
                         static_cast<uint32_t>(GraphicsMaterialWrites.size()),
                         GraphicsMaterialWrites.data(), 0, VK_NULL_HANDLE);
  Entry->TextureView = TextureView;
  Entry->TextureSource = TextureSource;
  Entry->Revision = MaterialRevision;
#if !defined(NDEBUG)
  ++m_DebugGraphicsMaterialDescriptorUpdates;
#endif
  return Entry->DescriptorSet;
}

#if !defined(NDEBUG)
void VulkanMaterialResources::ResetDebugCounters() {
  m_DebugGraphicsMaterialDescriptorUpdates = 0;
}
#endif
} // namespace Axiom
