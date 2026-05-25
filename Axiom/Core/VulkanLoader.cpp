#include "Core/VulkanLoader.h"

#include "HAL/DynamicLibrary.h"
#include "HAL/Platform.h"

#include "Core/Log.h"

#include <GLFW/glfw3.h>
#include <memory>
#include <string>
#include <vector>

namespace {
std::unique_ptr<Axiom::HAL::DynamicLibrary> g_CustomVulkanLoader;

[[nodiscard]] Axiom::VulkanLoaderInfo ResolveVulkanLoaderInfo() {
  Axiom::VulkanLoaderInfo Info{};
  A_CORE_INFO("Detected platform: {0}", Axiom::HAL::GetPlatformName());

  const VkResult DefaultLoaderResult = volkInitialize();
  if (DefaultLoaderResult == VK_SUCCESS) {
    Info.IsAvailable = true;
    Info.Source = "platform-default Vulkan loader";
    A_CORE_INFO("Using platform-default Vulkan loader discovery");
    return Info;
  }

  A_CORE_WARN("Platform-default Vulkan loader discovery failed with VkResult {0}",
              static_cast<int>(DefaultLoaderResult));

  const std::vector<std::string> Candidates =
      Axiom::HAL::GetDefaultVulkanLoaderCandidatePaths();
  for (const std::string &Candidate : Candidates) {
    if (Candidate.empty()) {
      continue;
    }

    auto Module = std::make_unique<Axiom::HAL::DynamicLibrary>();
    if (!Module->Open(Candidate.c_str())) {
      A_CORE_WARN("Failed to open Vulkan loader candidate '{0}': {1}",
                  Candidate, Axiom::HAL::DynamicLibrary::GetLastError());
      continue;
    }

    PFN_vkGetInstanceProcAddr ProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            Module->FindSymbol("vkGetInstanceProcAddr"));
    if (ProcAddr == nullptr) {
      A_CORE_WARN(
          "Vulkan loader candidate '{0}' did not export vkGetInstanceProcAddr",
          Candidate);
      continue;
    }

    volkInitializeCustom(ProcAddr);

    Info.ProcAddr = ProcAddr;
    Info.Source = Candidate;
    Info.UsesCustomLoader = true;
    Info.IsAvailable = true;
    g_CustomVulkanLoader = std::move(Module);

    A_CORE_INFO("Using custom Vulkan loader override: {0}", Candidate);

    return Info;
  }

  A_CORE_CRITICAL(
      "Unable to resolve a Vulkan loader. Checked platform-default discovery "
      "and {0} override/fallback candidates.",
      Candidates.size());
  return Info;
}
} // namespace

namespace Axiom {
const char *GetPlatformName() {
  return HAL::GetPlatformName();
}

const VulkanLoaderInfo &GetVulkanLoaderInfo() {
  static const VulkanLoaderInfo LoaderInfo = ResolveVulkanLoaderInfo();
  return LoaderInfo;
}

void ConfigureGlfwVulkanLoader() {
  const VulkanLoaderInfo &LoaderInfo = GetVulkanLoaderInfo();
  if (LoaderInfo.UsesCustomLoader && LoaderInfo.ProcAddr != nullptr) {
    glfwInitVulkanLoader(LoaderInfo.ProcAddr);
  }
}
} // namespace Axiom
