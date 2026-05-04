#include "Renderer/Vulkan/VulkanContext.h"

#include <volk.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <cstdlib>

#include "Core/Log.h"
#include "Core/VulkanLoader.h"
#include "Renderer/Vulkan/VulkanStringUtils.h"

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
      VkDebugSeverityToString(MessageSeverity), pCallbackData->pMessage);
  return VK_FALSE;
}
} // namespace

namespace Axiom {
void VulkanContext::Init(GLFWwindow *Window) {
  const VulkanLoaderInfo &LoaderInfo = GetVulkanLoaderInfo();
  if (!LoaderInfo.IsAvailable) {
    A_CORE_CRITICAL("Failed to resolve a Vulkan loader for Vulkan context init");
    Axiom::Log::Flush();
    std::abort();
  }

  A_CORE_INFO("Vulkan loader source: {0}",
              LoaderInfo.Source.empty() ? "platform-default Vulkan loader"
                                        : LoaderInfo.Source);
  A_CORE_INFO("Volk successfully initialized using {0}",
              LoaderInfo.UsesCustomLoader ? "custom loader" : "default loader");

  if (!glfwVulkanSupported()) {
    A_CORE_CRITICAL("GLFW reports Vulkan is not supported on this machine!");
    Axiom::Log::Flush();
    std::abort();
  }

  vkb::InstanceBuilder Builder = [&LoaderInfo]() {
    if (LoaderInfo.UsesCustomLoader) {
      return vkb::InstanceBuilder{LoaderInfo.ProcAddr};
    }
    return vkb::InstanceBuilder{};
  }();

  vkb::Result<vkb::SystemInfo> SystemInfoReturn = LoaderInfo.UsesCustomLoader
                                                      ? vkb::SystemInfo::get_system_info(
                                                            LoaderInfo.ProcAddr)
                                                      : vkb::SystemInfo::get_system_info();
  if (!SystemInfoReturn) {
    A_CORE_CRITICAL(
        "Failed to get device system info! This is a critical crash as we are "
        "unable to get GPU extensions without this.");
    Axiom::Log::Flush();
    std::abort();
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
    Axiom::Log::Flush();
    std::abort();
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

  const VkResult SurfaceResult =
      glfwCreateWindowSurface(Instance, Window, nullptr, &Surface);
  if (SurfaceResult != VK_SUCCESS) {
    A_CORE_CRITICAL("Failed to create Vulkan window surface: {0}",
                    VkResultToString(SurfaceResult));
    Axiom::Log::Flush();
    std::abort();
  }
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
