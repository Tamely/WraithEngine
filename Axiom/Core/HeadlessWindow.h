#pragma once

#include "Core/Window.h"

namespace Axiom {
class HeadlessWindow final : public Window {
public:
  HeadlessWindow(const std::string &Title, uint32_t Width, uint32_t Height);

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
  CursorMode m_CursorMode{CursorMode::Normal};
  bool m_ShouldClose{false};
};
} // namespace Axiom
