#include "Renderer/Vulkan/VulkanDevice.h"

#include "Renderer/Vulkan/VulkanContext.h"

#include <volk.h>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace Axiom {
void VulkanDevice::Init(VulkanContext &Context) {
  VkPhysicalDeviceVulkan13Features Features13{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  Features13.dynamicRendering = true;
  Features13.synchronization2 = true;

  VkPhysicalDeviceVulkan12Features Features12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  Features12.bufferDeviceAddress = true;
  Features12.descriptorIndexing = true;

  vkb::PhysicalDeviceSelector Selector{Context.BootstrapInstance};
  vkb::PhysicalDevice SelectedPhysicalDevice =
      Selector.set_minimum_version(1, 3)
          .set_required_features_13(Features13)
          .set_required_features_12(Features12)
          .set_surface(Context.Surface)
          .select()
          .value();

  vkb::DeviceBuilder DeviceBuilder{SelectedPhysicalDevice};
  vkb::Device VkbDevice = DeviceBuilder.build().value();

  Device = VkbDevice.device;
  PhysicalDevice = SelectedPhysicalDevice.physical_device;

  volkLoadDevice(Device);

  GraphicsQueue = VkbDevice.get_queue(vkb::QueueType::graphics).value();
  GraphicsQueueFamily =
      VkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  VmaVulkanFunctions VulkanFunctions = {};
  VulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  VulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo AllocatorInfo = {};
  AllocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
  AllocatorInfo.pVulkanFunctions = &VulkanFunctions;
  AllocatorInfo.device = Device;
  AllocatorInfo.physicalDevice = PhysicalDevice;
  AllocatorInfo.instance = Context.Instance;
  AllocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&AllocatorInfo, &Allocator);

  m_DeletionQueue.PushFunction([this]() { vmaDestroyAllocator(Allocator); });
}

void VulkanDevice::Shutdown() {
  m_DeletionQueue.Flush();

  if (Device != VK_NULL_HANDLE) {
    vkDestroyDevice(Device, VK_NULL_HANDLE);
    Device = VK_NULL_HANDLE;
  }
}
} // namespace Axiom
