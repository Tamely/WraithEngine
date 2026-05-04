#include "Core/GlfwWindow.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Core/Log.h"
#include "Core/VulkanLoader.h"

#include <cstdlib>

namespace Axiom {
GlfwWindow::GlfwWindow(const std::string &Title, uint32_t Width, uint32_t Height)
    : Window(Title, Width, Height) {
  glfwSetErrorCallback([](int Error, const char *Description) {
    A_CORE_ERROR("GLFW error {0}: {1}", Error, Description);
  });

  const VulkanLoaderInfo &LoaderInfo = GetVulkanLoaderInfo();
  if (!LoaderInfo.IsAvailable) {
    A_CORE_CRITICAL("Failed to resolve a Vulkan loader before GLFW startup");
    Axiom::Log::Flush();
    std::abort();
  }

  ConfigureGlfwVulkanLoader();

  if (!glfwInit()) {
    A_CORE_CRITICAL("GLFW initialization failed");
    Axiom::Log::Flush();
    std::abort();
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  m_NativeHandle =
      glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);

  if (!m_NativeHandle) {
    A_CORE_CRITICAL("Failed to create GLFW window '{0}'", m_Title);
    Axiom::Log::Flush();
    std::abort();
  }
}

GlfwWindow::~GlfwWindow() {
  if (m_NativeHandle != nullptr) {
    glfwDestroyWindow(m_NativeHandle);
  }
  glfwTerminate();
}

void GlfwWindow::PollEvents() { glfwPollEvents(); }

bool GlfwWindow::IsKeyPressed(int Key) const {
  return glfwGetKey(m_NativeHandle, Key) == GLFW_PRESS;
}

bool GlfwWindow::IsMouseButtonPressed(int Button) const {
  return glfwGetMouseButton(m_NativeHandle, Button) == GLFW_PRESS;
}

void GlfwWindow::GetCursorPosition(double &X, double &Y) const {
  glfwGetCursorPos(m_NativeHandle, &X, &Y);
}

void GlfwWindow::SetCursorMode(CursorMode Mode) {
  const int GlfwMode =
      Mode == CursorMode::Disabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
  glfwSetInputMode(m_NativeHandle, GLFW_CURSOR, GlfwMode);
}

CursorMode GlfwWindow::GetCursorMode() const {
  return glfwGetInputMode(m_NativeHandle, GLFW_CURSOR) == GLFW_CURSOR_DISABLED
             ? CursorMode::Disabled
             : CursorMode::Normal;
}

bool GlfwWindow::ShouldClose() const {
  return glfwWindowShouldClose(m_NativeHandle);
}

void GlfwWindow::RequestClose() { glfwSetWindowShouldClose(m_NativeHandle, 1); }

void *GlfwWindow::GetNativeHandle() const { return m_NativeHandle; }
} // namespace Axiom
