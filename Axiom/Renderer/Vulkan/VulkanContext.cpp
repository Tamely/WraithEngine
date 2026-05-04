#include "Renderer/Vulkan/VulkanContext.h"

#include <volk.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <cstdlib>
#include <dlfcn.h>

#include "Core/Log.h"
#include "Renderer/Vulkan/VulkanStringUtils.h"

namespace {
constexpr bool bUseValidationLayers = true;

PFN_vkGetInstanceProcAddr LoadVulkanLoaderProcAddr() {
#ifdef AXIOM_VULKAN_LOADER_LIBRARY
  void *Loader = dlopen(AXIOM_VULKAN_LOADER_LIBRARY, RTLD_NOW | RTLD_LOCAL);
  if (!Loader) {
    A_CORE_CRITICAL("Failed to load Vulkan loader '{0}': {1}",
                    AXIOM_VULKAN_LOADER_LIBRARY, dlerror());
    return nullptr;
  }

  return reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      dlsym(Loader, "vkGetInstanceProcAddr"));
#else
  return nullptr;
#endif
}

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
  PFN_vkGetInstanceProcAddr LoaderProcAddr = LoadVulkanLoaderProcAddr();
  if (LoaderProcAddr == nullptr) {
    A_CORE_CRITICAL("Failed to load vkGetInstanceProcAddr from Vulkan loader");
    Axiom::Log::Flush();
    abort();
  }

  volkInitializeCustom(LoaderProcAddr);
  A_CORE_INFO("Volk successfully initialized!");

  glfwInitVulkanLoader(LoaderProcAddr);
  if (!glfwVulkanSupported()) {
    A_CORE_CRITICAL("GLFW reports Vulkan is not supported on this machine!");
    Axiom::Log::Flush();
    abort();
  }

  vkb::InstanceBuilder Builder{LoaderProcAddr};

  vkb::Result<vkb::SystemInfo> SystemInfoReturn =
      vkb::SystemInfo::get_system_info(LoaderProcAddr);
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
