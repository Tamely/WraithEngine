#include "Core/VulkanLoader.h"

#include "Core/Log.h"
#include "Core/Platform.h"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#if AXIOM_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
constexpr std::string_view kLoaderEnvironmentVariable =
    "AXIOM_VULKAN_LOADER_PATH";

#ifdef AXIOM_VULKAN_LOADER_PATH
constexpr const char *kConfiguredLoaderPath = AXIOM_VULKAN_LOADER_PATH;
#else
constexpr const char *kConfiguredLoaderPath = nullptr;
#endif

struct VulkanLoaderModule {
#if AXIOM_PLATFORM_WINDOWS
  HMODULE Handle{nullptr};
#else
  void *Handle{nullptr};
#endif

  [[nodiscard]] bool IsValid() const { return Handle != nullptr; }
};

[[nodiscard]] std::string GetEnvironmentVariable(
    std::string_view VariableName) {
  if (const char *Value = std::getenv(std::string(VariableName).c_str())) {
    return Value;
  }

  return {};
}

[[nodiscard]] VulkanLoaderModule OpenDynamicLibrary(const char *Path) {
  VulkanLoaderModule Module{};

#if AXIOM_PLATFORM_WINDOWS
  Module.Handle = LoadLibraryA(Path);
#else
  Module.Handle = dlopen(Path, RTLD_NOW | RTLD_LOCAL);
#endif

  return Module;
}

[[nodiscard]] std::string GetLastDynamicLibraryError() {
#if AXIOM_PLATFORM_WINDOWS
  const DWORD ErrorCode = GetLastError();
  if (ErrorCode == 0) {
    return "unknown Windows loader error";
  }

  LPSTR MessageBuffer = nullptr;
  const DWORD MessageLength = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, ErrorCode, 0, reinterpret_cast<LPSTR>(&MessageBuffer), 0,
      nullptr);
  std::string Message =
      MessageLength > 0 && MessageBuffer != nullptr ? MessageBuffer
                                                    : "unknown Windows loader error";
  if (MessageBuffer != nullptr) {
    LocalFree(MessageBuffer);
  }
  return Message;
#else
  if (const char *Error = dlerror()) {
    return Error;
  }

  return "unknown POSIX loader error";
#endif
}

[[nodiscard]] PFN_vkGetInstanceProcAddr LoadVkGetInstanceProcAddr(
    const VulkanLoaderModule &Module) {
#if AXIOM_PLATFORM_WINDOWS
  return reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      GetProcAddress(Module.Handle, "vkGetInstanceProcAddr"));
#else
  return reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      dlsym(Module.Handle, "vkGetInstanceProcAddr"));
#endif
}

[[nodiscard]] std::vector<std::string> GetLoaderCandidatePaths() {
  std::vector<std::string> Candidates;

  if (kConfiguredLoaderPath != nullptr && kConfiguredLoaderPath[0] != '\0') {
    Candidates.emplace_back(kConfiguredLoaderPath);
  }

  std::string EnvironmentPath = GetEnvironmentVariable(kLoaderEnvironmentVariable);
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

[[nodiscard]] Axiom::VulkanLoaderInfo ResolveVulkanLoaderInfo() {
  Axiom::VulkanLoaderInfo Info{};
  A_CORE_INFO("Detected platform: {0}", Axiom::GetPlatformName());

  const VkResult DefaultLoaderResult = volkInitialize();
  if (DefaultLoaderResult == VK_SUCCESS) {
    Info.IsAvailable = true;
    Info.Source = "platform-default Vulkan loader";
    A_CORE_INFO("Using platform-default Vulkan loader discovery");
    return Info;
  }

  A_CORE_WARN("Platform-default Vulkan loader discovery failed with VkResult {0}",
              static_cast<int>(DefaultLoaderResult));

  const std::vector<std::string> Candidates = GetLoaderCandidatePaths();
  for (const std::string &Candidate : Candidates) {
    if (Candidate.empty()) {
      continue;
    }

    VulkanLoaderModule Module = OpenDynamicLibrary(Candidate.c_str());
    if (!Module.IsValid()) {
      A_CORE_WARN("Failed to open Vulkan loader candidate '{0}': {1}",
                  Candidate, GetLastDynamicLibraryError());
      continue;
    }

    PFN_vkGetInstanceProcAddr ProcAddr = LoadVkGetInstanceProcAddr(Module);
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

#if AXIOM_PLATFORM_MACOS
    if (Candidate.find("MoltenVK") != std::string::npos ||
        Candidate.find("macOS/lib") != std::string::npos) {
      A_CORE_INFO("Using macOS Vulkan loader fallback: {0}", Candidate);
    } else {
      A_CORE_INFO("Using custom Vulkan loader override: {0}", Candidate);
    }
#else
    A_CORE_INFO("Using custom Vulkan loader override: {0}", Candidate);
#endif

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
