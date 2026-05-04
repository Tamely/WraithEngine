#pragma once

#include "Core/CursorMode.h"
#include "Renderer/RenderSurface.h"

#include <cstdint>
#include <string>

namespace Axiom {
class Window final : public IRenderSurface {
public:
  Window(std::string Title, uint32_t Width, uint32_t Height);
  virtual ~Window() = default;

  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;

  virtual void PollEvents() = 0;
  virtual bool IsKeyPressed(int Key) const = 0;
  virtual bool IsMouseButtonPressed(int Button) const = 0;
  virtual void GetCursorPosition(double &X, double &Y) const = 0;
  virtual void SetCursorMode(CursorMode Mode) = 0;
  [[nodiscard]] virtual CursorMode GetCursorMode() const = 0;
  [[nodiscard]] virtual bool ShouldClose() const = 0;
  virtual void RequestClose() = 0;
  [[nodiscard]] virtual void *GetNativeHandle() const = 0;

  [[nodiscard]] uint32_t GetWidth() const { return m_Width; }
  [[nodiscard]] uint32_t GetHeight() const { return m_Height; }
  [[nodiscard]] const std::string &GetTitle() const { return m_Title; }

protected:
  std::string m_Title;
  uint32_t m_Width;
  uint32_t m_Height;
};
} // namespace Axiom
