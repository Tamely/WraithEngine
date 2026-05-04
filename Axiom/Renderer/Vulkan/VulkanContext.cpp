#include "Renderer/Vulkan/VulkanContext.h"

#include <volk.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <vulkan/vk_enum_string_helper.h>

#include "Core/Log.h"

namespace {
constexpr bool bUseValidationLayers = true;

static VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT MessageType,
                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void *pUserData) {
  (void)MessageType;
  (void)pUserData;

  A_CORE_CRITICAL(
      "Vulkan Validation: [Severity: {0}]: {1}",
      string_VkDebugUtilsMessageSeverityFlagBitsEXT(MessageSeverity),
      pCallbackData->pMessage);
  return VK_FALSE;
}
} // namespace

namespace Axiom {
void VulkanContext::Init(GLFWwindow *Window) {
  if (!glfwVulkanSupported()) {
    A_CORE_CRITICAL("GLFW reports Vulkan is not supported on this machine!");
    return;
  }

  VkResult VolkResult = volkInitialize();
  if (VolkResult != VK_SUCCESS) {
    A_CORE_CRITICAL("Failed to initializes Volk: {0}",
                    static_cast<int>(VolkResult));
    return;
  }
  A_CORE_INFO("Volk successfully initialized!");

  vkb::InstanceBuilder Builder;

  vkb::Result<vkb::SystemInfo> SystemInfoReturn =
      vkb::SystemInfo::get_system_info();
  if (!SystemInfoReturn) {
    A_CORE_CRITICAL(
        "Failed to get device system info! This is a critical crash as we are "
        "unable to get GPU extensions without this.");
    return;
  }
  vkb::SystemInfo SystemInfo = SystemInfoReturn.value();

  if (SystemInfo.is_extension_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    Builder.enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  } else {
    A_CORE_WARN("Debug utils extension is not available!");
  }

  auto InstanceReturn = Builder.set_app_name("Axiom Engine")
                            .request_validation_layers(bUseValidationLayers)
                            .set_debug_callback(VulkanDebugCallback)
                            .require_api_version(1, 3, 0)
                            .build();

  if (!InstanceReturn) {
    A_CORE_CRITICAL("Failed to create instance: {0}",
                    InstanceReturn.error().message());
    return;
  }

  BootstrapInstance = InstanceReturn.value();

  Instance = BootstrapInstance.instance;
  DebugMessenger = BootstrapInstance.debug_messenger;

  if (DebugMessenger == VK_NULL_HANDLE) {
    A_CORE_WARN("Debug messenger was not created!");
  } else {
    A_CORE_INFO("Debug messenger created successfully");
  }

  volkLoadInstance(Instance);

  glfwCreateWindowSurface(Instance, Window, nullptr, &Surface);
}

void VulkanContext::Shutdown() {
  m_DeletionQueue.Flush();

  if (Surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(Instance, Surface, VK_NULL_HANDLE);
    Surface = VK_NULL_HANDLE;
  }

  if (Instance != VK_NULL_HANDLE) {
    vkb::destroy_debug_utils_messenger(Instance, DebugMessenger);
    vkDestroyInstance(Instance, VK_NULL_HANDLE);
    Instance = VK_NULL_HANDLE;
  }
}
} // namespace Axiom

