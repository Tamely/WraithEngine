#pragma once

#include <string>

#include <volk.h>

namespace Axiom {
struct VulkanLoaderInfo {
  PFN_vkGetInstanceProcAddr ProcAddr{nullptr};
  std::string Source;
  bool UsesCustomLoader{false};
  bool IsAvailable{false};
};

[[nodiscard]] const char *GetPlatformName();
[[nodiscard]] const VulkanLoaderInfo &GetVulkanLoaderInfo();
void ConfigureGlfwVulkanLoader();
} // namespace Axiom
