#pragma once

#include <volk.h>

namespace VkUtil {
void TransitionImage(VkCommandBuffer Command, VkImage Image,
                     VkImageLayout CurrentLayout, VkImageLayout NewLayout);

void CopyImageToImage(VkCommandBuffer Command, VkImage Source,
                      VkImage Destination, VkExtent2D SrcSize,
                      VkExtent2D DstSize);
} // namespace VkUtil

