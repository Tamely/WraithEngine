#pragma once

#include "Renderer/RenderSurface.h"

namespace Axiom {
class OffscreenRenderSurface final : public IRenderSurface {
public:
  OffscreenRenderSurface(uint32_t Width, uint32_t Height)
      : m_Width(Width), m_Height(Height) {}

  [[nodiscard]] RenderSurfaceKind GetKind() const override {
    return RenderSurfaceKind::Offscreen;
  }

  [[nodiscard]] uint32_t GetWidth() const override { return m_Width; }
  [[nodiscard]] uint32_t GetHeight() const override { return m_Height; }
  void PollEvents() override {}
  [[nodiscard]] bool ShouldClose() const override { return false; }

private:
  uint32_t m_Width{0};
  uint32_t m_Height{0};
};
} // namespace Axiom
