#include "Renderer/Vulkan/VulkanSwapchain.h"

#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanDevice.h"

#include <VkBootstrap.h>

namespace Axiom {
void VulkanSwapchain::Init(VulkanContext &Context, VulkanDevice &Device,
                           uint32_t Width, uint32_t Height) {
  vkb::SwapchainBuilder SwapchainBuilder{Device.PhysicalDevice, Device.Device,
                                         Context.Surface};
  ImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

  vkb::Swapchain VkbSwapchain =
      SwapchainBuilder
          .set_desired_format(VkSurfaceFormatKHR{
              .format = ImageFormat,
              .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(Width, Height)
          .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
          .build()
          .value();

  Extent = VkbSwapchain.extent;
  Swapchain = VkbSwapchain.swapchain;
  Images = VkbSwapchain.get_images().value();
  ImageViews = VkbSwapchain.get_image_views().value();
}

void VulkanSwapchain::Shutdown(VulkanDevice &Device) {
  if (Swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(Device.Device, Swapchain, VK_NULL_HANDLE);
    Swapchain = VK_NULL_HANDLE;
  }

  for (VkImageView ImageView : ImageViews) {
    vkDestroyImageView(Device.Device, ImageView, VK_NULL_HANDLE);
  }

  Images.clear();
  ImageViews.clear();
}

uint32_t VulkanSwapchain::AcquireNextImage(VkDevice Device,
                                           VkSemaphore Semaphore) const {
  uint32_t ImageIndex = 0;
  VK_CHECK(vkAcquireNextImageKHR(Device, Swapchain, 1000000000, Semaphore,
                                 VK_NULL_HANDLE, &ImageIndex));
  return ImageIndex;
}

void VulkanSwapchain::Present(VkQueue Queue, uint32_t ImageIndex,
                              VkSemaphore WaitSemaphore) const {
  VkPresentInfoKHR PresentInfo = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                  .pNext = VK_NULL_HANDLE};
  PresentInfo.pSwapchains = &Swapchain;
  PresentInfo.swapchainCount = 1;
  PresentInfo.pWaitSemaphores = &WaitSemaphore;
  PresentInfo.waitSemaphoreCount = 1;
  PresentInfo.pImageIndices = &ImageIndex;

  VK_CHECK(vkQueuePresentKHR(Queue, &PresentInfo));
}
} // namespace Axiom

