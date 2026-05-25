#include "HAL/Platform.h"

#include <cstdlib>

namespace {
constexpr std::string_view kVulkanLoaderEnvironmentVariable =
    "AXIOM_VULKAN_LOADER_PATH";

#ifdef AXIOM_VULKAN_LOADER_PATH
constexpr const char *kConfiguredVulkanLoaderPath = AXIOM_VULKAN_LOADER_PATH;
#else
constexpr const char *kConfiguredVulkanLoaderPath = nullptr;
#endif
} // namespace

namespace Axiom::HAL {
const char *GetPlatformName() {
#if AXIOM_PLATFORM_WINDOWS
  return "Windows";
#elif AXIOM_PLATFORM_MACOS
  return "macOS";
#elif AXIOM_PLATFORM_LINUX
  return "Linux";
#else
  return "Unknown";
#endif
}

std::string GetEnvironmentVariable(std::string_view VariableName) {
  const std::string Name(VariableName);
  if (const char *Value = std::getenv(Name.c_str())) {
    return Value;
  }
  return {};
}

std::vector<std::string> GetDefaultVulkanLoaderCandidatePaths() {
  std::vector<std::string> Candidates;

  if (kConfiguredVulkanLoaderPath != nullptr &&
      kConfiguredVulkanLoaderPath[0] != '\0') {
    Candidates.emplace_back(kConfiguredVulkanLoaderPath);
  }

  std::string EnvironmentPath =
      GetEnvironmentVariable(kVulkanLoaderEnvironmentVariable);
  if (!EnvironmentPath.empty() &&
      (Candidates.empty() || Candidates.front() != EnvironmentPath)) {
    Candidates.emplace_back(std::move(EnvironmentPath));
  }

#if AXIOM_PLATFORM_MACOS
  const std::string VulkanSdk = GetEnvironmentVariable("VULKAN_SDK");
  if (!VulkanSdk.empty()) {
    Candidates.emplace_back(VulkanSdk + "/macOS/lib/libvulkan.1.dylib");
    Candidates.emplace_back(VulkanSdk + "/macOS/lib/libvulkan.dylib");
    Candidates.emplace_back(VulkanSdk + "/macOS/lib/libMoltenVK.dylib");
  }

  Candidates.emplace_back("/usr/local/lib/libvulkan.1.dylib");
  Candidates.emplace_back("/usr/local/lib/libvulkan.dylib");
  Candidates.emplace_back("/usr/local/lib/libMoltenVK.dylib");
  Candidates.emplace_back("/opt/homebrew/lib/libvulkan.1.dylib");
  Candidates.emplace_back("/opt/homebrew/lib/libvulkan.dylib");
  Candidates.emplace_back("/opt/homebrew/lib/libMoltenVK.dylib");
#endif

  return Candidates;
}
} // namespace Axiom::HAL
