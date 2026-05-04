#pragma once

#include "Core/CursorMode.h"
#include "Renderer/RenderSurface.h"

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace Axiom {
class Window final : public IRenderSurface {
public:
  Window(const std::string &Title, uint32_t Width, uint32_t Height);
  ~Window();

  Window(Window &) = delete;
  void operator=(Window &) = delete;

public:
  [[nodiscard]] RenderSurfaceKind GetKind() const override;
  void PollEvents() override;
  bool IsKeyPressed(int Key) const;
  bool IsMouseButtonPressed(int Button) const;
  void GetCursorPosition(double &X, double &Y) const;
  void SetCursorMode(CursorMode Mode);
  [[nodiscard]] CursorMode GetCursorMode() const;

  [[nodiscard]] bool ShouldClose() const override;
  [[nodiscard]] bool IsMinimized() const override;
  [[nodiscard]] GLFWwindow *GetGlfwWindow() const override { return m_NativeHandle; }
  [[nodiscard]] GLFWwindow *GetNativeHandle() const { return m_NativeHandle; }
  [[nodiscard]] uint32_t GetWidth() const override { return m_Width; }
  [[nodiscard]] uint32_t GetHeight() const override { return m_Height; }

private:
  std::string m_Title;
  uint32_t m_Width;
  uint32_t m_Height;

  GLFWwindow *m_NativeHandle{nullptr};
};
} // namespace Axiom
