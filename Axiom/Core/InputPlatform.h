#pragma once

#include "Core/CursorMode.h"

#include <glm/vec2.hpp>

namespace Axiom {
class IInputPlatform {
public:
  virtual ~IInputPlatform() = default;

  virtual bool IsKeyPressed(int Key) const = 0;
  virtual bool IsMouseButtonPressed(int Button) const = 0;
  virtual glm::dvec2 GetCursorPosition() const = 0;
  virtual void SetCursorMode(CursorMode Mode) = 0;
  [[nodiscard]] virtual CursorMode GetCursorMode() const = 0;
};
} // namespace Axiom
