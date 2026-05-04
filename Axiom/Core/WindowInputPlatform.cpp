#include "Core/WindowInputPlatform.h"

namespace Axiom {
bool WindowInputPlatform::IsKeyPressed(int Key) const {
  return m_Window.IsKeyPressed(Key);
}

bool WindowInputPlatform::IsMouseButtonPressed(int Button) const {
  return m_Window.IsMouseButtonPressed(Button);
}

glm::dvec2 WindowInputPlatform::GetCursorPosition() const {
  double X = 0.0;
  double Y = 0.0;
  m_Window.GetCursorPosition(X, Y);
  return {X, Y};
}

void WindowInputPlatform::SetCursorMode(CursorMode Mode) {
  m_Window.SetCursorMode(Mode);
}

CursorMode WindowInputPlatform::GetCursorMode() const {
  return m_Window.GetCursorMode();
}
} // namespace Axiom
