#pragma once

#include "Core/Window.h"

#include <cstdint>
#include <memory>

namespace Axiom {
enum class RenderSurfaceKind { Window, Offscreen };

class IRenderSurface {
public:
  virtual ~IRenderSurface() = default;

  [[nodiscard]] virtual RenderSurfaceKind GetKind() const = 0;
  [[nodiscard]] virtual bool SupportsPresentation() const = 0;
  [[nodiscard]] virtual uint32_t GetWidth() const = 0;
  [[nodiscard]] virtual uint32_t GetHeight() const = 0;
  [[nodiscard]] virtual void *GetNativeWindowHandle() const = 0;
};

class WindowRenderSurface final : public IRenderSurface {
public:
  explicit WindowRenderSurface(Window &TargetWindow) : m_TargetWindow(TargetWindow) {}

  [[nodiscard]] RenderSurfaceKind GetKind() const override {
    return RenderSurfaceKind::Window;
  }
  [[nodiscard]] bool SupportsPresentation() const override { return true; }
  [[nodiscard]] uint32_t GetWidth() const override {
    return m_TargetWindow.GetWidth();
  }
  [[nodiscard]] uint32_t GetHeight() const override {
    return m_TargetWindow.GetHeight();
  }
  [[nodiscard]] void *GetNativeWindowHandle() const override {
    return m_TargetWindow.GetNativeHandle();
  }

private:
  Window &m_TargetWindow;
};

class OffscreenRenderSurface final : public IRenderSurface {
public:
  OffscreenRenderSurface(uint32_t Width, uint32_t Height)
      : m_Width(Width), m_Height(Height) {}

  [[nodiscard]] RenderSurfaceKind GetKind() const override {
    return RenderSurfaceKind::Offscreen;
  }
  [[nodiscard]] bool SupportsPresentation() const override { return false; }
  [[nodiscard]] uint32_t GetWidth() const override { return m_Width; }
  [[nodiscard]] uint32_t GetHeight() const override { return m_Height; }
  [[nodiscard]] void *GetNativeWindowHandle() const override { return nullptr; }

private:
  uint32_t m_Width{0};
  uint32_t m_Height{0};
};

using RenderSurfacePtr = std::shared_ptr<IRenderSurface>;
} // namespace Axiom
