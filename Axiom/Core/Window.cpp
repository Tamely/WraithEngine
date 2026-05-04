#include "Window.h"
#include <iostream>

#include <GLFW/glfw3.h>

#include <string>

namespace Axiom {
Window::Window(const std::string &Title, uint32_t Width, uint32_t Height)
    : m_Title(Title), m_Width(Width), m_Height(Height) {
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

[[nodiscard]] bool Window::ShouldClose() const {
  return glfwWindowShouldClose(m_NativeHandle);
}
} // namespace Axiom

