#include "Window.h"
#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <volk.h>

#include "Core/Log.h"

#include <dlfcn.h>
#include <string>

namespace {
PFN_vkGetInstanceProcAddr LoadVulkanLoaderProcAddr() {
#ifdef AXIOM_VULKAN_LOADER_LIBRARY
  void *Loader = dlopen(AXIOM_VULKAN_LOADER_LIBRARY, RTLD_NOW | RTLD_LOCAL);
  if (!Loader) {
    A_CORE_ERROR("Failed to load Vulkan loader '{0}': {1}",
                 AXIOM_VULKAN_LOADER_LIBRARY, dlerror());
    return nullptr;
  }

  return reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      dlsym(Loader, "vkGetInstanceProcAddr"));
#else
  return nullptr;
#endif
}
} // namespace

namespace Axiom {
Window::Window(const std::string &Title, uint32_t Width, uint32_t Height)
    : m_Title(Title), m_Width(Width), m_Height(Height) {
  glfwSetErrorCallback([](int Error, const char *Description) {
    A_CORE_ERROR("GLFW error {0}: {1}", Error, Description);
  });

  if (PFN_vkGetInstanceProcAddr LoaderProcAddr = LoadVulkanLoaderProcAddr()) {
    glfwInitVulkanLoader(LoaderProcAddr);
  }

  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  m_NativeHandle =
      glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);

  if (!m_NativeHandle) {
    std::cerr << "Failed to create window!" << std::endl;
    return;
  }

}

Window::~Window() {
  glfwDestroyWindow(m_NativeHandle);
  glfwTerminate();
}

void Window::PollEvents() { glfwPollEvents(); }

bool Window::IsKeyPressed(int Key) const {
  return glfwGetKey(m_NativeHandle, Key) == GLFW_PRESS;
}

bool Window::IsMouseButtonPressed(int Button) const {
  return glfwGetMouseButton(m_NativeHandle, Button) == GLFW_PRESS;
}

void Window::GetCursorPosition(double &X, double &Y) const {
  glfwGetCursorPos(m_NativeHandle, &X, &Y);
}

void Window::SetCursorMode(CursorMode Mode) {
  const int GlfwMode =
      Mode == CursorMode::Disabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
  glfwSetInputMode(m_NativeHandle, GLFW_CURSOR, GlfwMode);
}

CursorMode Window::GetCursorMode() const {
  return glfwGetInputMode(m_NativeHandle, GLFW_CURSOR) == GLFW_CURSOR_DISABLED
             ? CursorMode::Disabled
             : CursorMode::Normal;
}

[[nodiscard]] bool Window::ShouldClose() const {
  return glfwWindowShouldClose(m_NativeHandle);
}
} // namespace Axiom
