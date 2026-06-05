#pragma once

#include "Core/Window.h"
#include "Session/SessionTypes.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace Axiom {
enum class RendererViewMode : uint32_t {
  Lit = 0,
  Unlit = 1,
  Wireframe = 2,
};

enum class RenderSurfaceKind { Window, Offscreen };

class IRenderSurface {
public:
  virtual ~IRenderSurface() = default;

  [[nodiscard]] virtual RenderSurfaceKind GetKind() const = 0;
  [[nodiscard]] virtual bool SupportsPresentation() const = 0;
  [[nodiscard]] virtual uint32_t GetWidth() const = 0;
  [[nodiscard]] virtual uint32_t GetHeight() const = 0;
  [[nodiscard]] virtual bool IsMinimized() const = 0;
  [[nodiscard]] virtual void *GetNativeWindowHandle() const = 0;
  [[nodiscard]] virtual bool
  SupportsPresentationBackend(PresentationBackendType Backend) const = 0;
  virtual PresentationSurfaceResult
  CreatePresentationSurface(PresentationBackendType Backend, void *Instance,
                            void *Surface) const = 0;
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
  [[nodiscard]] bool IsMinimized() const override {
    return m_TargetWindow.IsMinimized();
  }
  [[nodiscard]] void *GetNativeWindowHandle() const override {
    return m_TargetWindow.GetNativeHandle();
  }
  [[nodiscard]] bool
  SupportsPresentationBackend(PresentationBackendType Backend) const override {
    return m_TargetWindow.SupportsPresentationBackend(Backend);
  }
  PresentationSurfaceResult
  CreatePresentationSurface(PresentationBackendType Backend, void *Instance,
                            void *Surface) const override {
    return m_TargetWindow.CreatePresentationSurface(Backend, Instance, Surface);
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
  [[nodiscard]] bool IsMinimized() const override { return false; }
  [[nodiscard]] void *GetNativeWindowHandle() const override { return nullptr; }
  [[nodiscard]] bool
  SupportsPresentationBackend(PresentationBackendType Backend) const override {
    (void)Backend;
    return false;
  }
  PresentationSurfaceResult
  CreatePresentationSurface(PresentationBackendType Backend, void *Instance,
                            void *Surface) const override {
    (void)Backend;
    (void)Instance;
    (void)Surface;
    return PresentationSurfaceResult::InitializationFailed;
  }

private:
  uint32_t m_Width{0};
  uint32_t m_Height{0};
};

using RenderSurfacePtr = std::shared_ptr<IRenderSurface>;

enum class ViewportFrameFormat : uint8_t {
  R16G16B16A16Float,
  R8G8B8A8Unorm,
};

struct ViewportFrame {
  uint64_t FrameIndex{0};
  uint32_t Width{0};
  uint32_t Height{0};
  ViewportFrameFormat Format{ViewportFrameFormat::R16G16B16A16Float};
  std::span<const std::byte> Pixels;
  SessionUserId User{};
};

class IViewportFrameOutput {
public:
  virtual ~IViewportFrameOutput() = default;
  virtual void OnViewportFrame(const ViewportFrame &Frame) = 0;
};
} // namespace Axiom
