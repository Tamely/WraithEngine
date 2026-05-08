#pragma once

#include "Core/InputPlatform.h"
#include "Core/Window.h"

namespace Axiom {
class WindowInputPlatform final : public IInputPlatform {
public:
  explicit WindowInputPlatform(Window &Window) : m_Window(Window) {}

  bool IsKeyPressed(int Key) const override;
  bool IsMouseButtonPressed(int Button) const override;
  glm::dvec2 GetCursorPosition() const override;
  void SetCursorMode(CursorMode Mode) override;
  [[nodiscard]] CursorMode GetCursorMode() const override;

private:
  Window &m_Window;
};
} // namespace Axiom
