#pragma once

#include "Core/Window.h"

struct GLFWwindow;

namespace Axiom {
class GlfwWindow final : public Window {
public:
  GlfwWindow(const std::string &Title, uint32_t Width, uint32_t Height);
  ~GlfwWindow() override;

  void PollEvents() override;
  bool IsKeyPressed(int Key) const override;
  bool IsMouseButtonPressed(int Button) const override;
  void GetCursorPosition(double &X, double &Y) const override;
  void SetCursorMode(CursorMode Mode) override;
  [[nodiscard]] CursorMode GetCursorMode() const override;
  [[nodiscard]] bool ShouldClose() const override;
  void RequestClose() override;
  [[nodiscard]] void *GetNativeHandle() const override;

private:
  GLFWwindow *m_NativeHandle{nullptr};
};
} // namespace Axiom
