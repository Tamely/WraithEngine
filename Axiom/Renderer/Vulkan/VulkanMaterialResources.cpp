#include "Renderer/Vulkan/VulkanMaterialResources.h"

#include "Renderer/Vulkan/VulkanBuffer.h"
#include "Renderer/Vulkan/VulkanDeletionQueue.h"
#include "Renderer/Vulkan/VulkanImage.h"
#include "Renderer/Vulkan/VulkanInitializers.h"

#include <array>
#include <cassert>
#include <cstring>

namespace Axiom {
void VulkanMaterialResources::Init(const CreateInfo &CreateInfo) {
  m_Allocator = CreateInfo.Allocator;
  m_Device = CreateInfo.Device;
  m_GraphicsQueue = CreateInfo.GraphicsQueue;
  m_DeletionQueue = CreateInfo.DeletionQueue;
  m_ImmediateSubmit = CreateInfo.ImmediateSubmit;
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

  m_FallbackTexture = CreateTextureImage(CheckerTexture);
}

AllocatedImage
VulkanMaterialResources::CreateTextureImage(const TextureSourceData &TextureData) {
  assert(TextureData.IsValid());

  AllocatedImage TextureImage{};
  TextureImage.ImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
  TextureImage.ImageExtent = {TextureData.Width, TextureData.Height, 1};

  VkImageCreateInfo ImageInfo = VkInit::ImageCreateInfo(
      TextureImage.ImageFormat,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      TextureImage.ImageExtent);

  VmaAllocationCreateInfo AllocationInfo{};
  AllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  AllocationInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VK_CHECK(vmaCreateImage(m_Allocator, &ImageInfo, &AllocationInfo,
                          &TextureImage.Image, &TextureImage.Allocation,
                          VK_NULL_HANDLE));

  VkImageViewCreateInfo ViewInfo = VkInit::ImageViewCreateInfo(
      TextureImage.ImageFormat, TextureImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device, &ViewInfo, VK_NULL_HANDLE,
                             &TextureImage.ImageView));

  auto StagingBuffer = VkBufferUtil::CreateBuffer(
      m_Allocator, TextureData.Pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_CPU_ONLY,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT);
  std::memcpy(StagingBuffer.Info.pMappedData, TextureData.Pixels.data(),
              TextureData.Pixels.size());

  m_ImmediateSubmit([&](VkCommandBuffer CommandBuffer) {
    VkUtil::TransitionImage(CommandBuffer, TextureImage.Image,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy CopyRegion{};
    CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    CopyRegion.imageSubresource.layerCount = 1;
    CopyRegion.imageExtent = TextureImage.ImageExtent;

    vkCmdCopyBufferToImage(CommandBuffer, StagingBuffer.Buffer, TextureImage.Image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &CopyRegion);

    VkUtil::TransitionImage(CommandBuffer, TextureImage.Image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  });

  VkBufferUtil::DestroyBuffer(m_Allocator, StagingBuffer);

  const AllocatedImage UploadedImage = TextureImage;
  m_DeletionQueue->PushFunction([this, UploadedImage]() mutable {
    vkDestroyImageView(m_Device, UploadedImage.ImageView, VK_NULL_HANDLE);
    vmaDestroyImage(m_Allocator, UploadedImage.Image, UploadedImage.Allocation);
  });

  return TextureImage;
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
      CreateTextureImage(*Material->BaseColorTexture);
  m_MaterialImageViews.emplace(Material.get(), TextureImage.ImageView);
  return TextureImage.ImageView;
}
} // namespace Axiom
