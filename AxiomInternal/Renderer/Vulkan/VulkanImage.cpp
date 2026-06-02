#include <volk.h>

#include "Renderer/Vulkan/VulkanImage.h"

#include "Renderer/Vulkan/VulkanInitializers.h"

namespace VkUtil {
namespace {
bool IsDepthLayout(VkImageLayout Layout) {
  switch (Layout) {
  case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
  case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
  case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
  case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    return true;
  default:
    return false;
  }
}
} // namespace

void TransitionImage(VkCommandBuffer Command, VkImage Image,
                     VkImageLayout CurrentLayout, VkImageLayout NewLayout) {
  VkImageMemoryBarrier2 ImageBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  ImageBarrier.pNext = VK_NULL_HANDLE;

  ImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  ImageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  ImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  ImageBarrier.dstAccessMask =
      VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  ImageBarrier.oldLayout = CurrentLayout;
  ImageBarrier.newLayout = NewLayout;

  VkImageAspectFlags AspectMask =
      (IsDepthLayout(CurrentLayout) || IsDepthLayout(NewLayout))
          ? VK_IMAGE_ASPECT_DEPTH_BIT
          : VK_IMAGE_ASPECT_COLOR_BIT;
  ImageBarrier.subresourceRange = VkInit::ImageSubresourceRange(AspectMask);
  ImageBarrier.image = Image;

  VkDependencyInfo DepInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                           .pNext = VK_NULL_HANDLE};

  DepInfo.imageMemoryBarrierCount = 1;
  DepInfo.pImageMemoryBarriers = &ImageBarrier;

  vkCmdPipelineBarrier2(Command, &DepInfo);
}

void CopyImageToImage(VkCommandBuffer Command, VkImage Source,
                      VkImage Destination, VkExtent2D SrcSize,
                      VkExtent2D DstSize) {
  VkImageBlit2 BlitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                          .pNext = VK_NULL_HANDLE};

  BlitRegion.srcOffsets[1].x = SrcSize.width;
  BlitRegion.srcOffsets[1].y = SrcSize.height;
  BlitRegion.srcOffsets[1].z = 1;

  BlitRegion.dstOffsets[1].x = DstSize.width;
  BlitRegion.dstOffsets[1].y = DstSize.height;
  BlitRegion.dstOffsets[1].z = 1;

  BlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  BlitRegion.srcSubresource.baseArrayLayer = 0;
  BlitRegion.srcSubresource.layerCount = 1;
  BlitRegion.srcSubresource.mipLevel = 0;

  BlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  BlitRegion.dstSubresource.baseArrayLayer = 0;
  BlitRegion.dstSubresource.layerCount = 1;
  BlitRegion.dstSubresource.mipLevel = 0;

  VkBlitImageInfo2 BlitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                            .pNext = VK_NULL_HANDLE};
  BlitInfo.dstImage = Destination;
  BlitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  BlitInfo.srcImage = Source;
  BlitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  BlitInfo.filter = VK_FILTER_LINEAR;
  BlitInfo.regionCount = 1;
  BlitInfo.pRegions = &BlitRegion;

  vkCmdBlitImage2(Command, &BlitInfo);
}
} // namespace VkUtil
