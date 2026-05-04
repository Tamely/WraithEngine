#pragma once

#include <cstdint>

struct GLFWwindow;

namespace Axiom {
enum class RenderSurfaceKind : uint8_t { Window, Offscreen };

class IRenderSurface {
public:
  virtual ~IRenderSurface() = default;

  [[nodiscard]] virtual RenderSurfaceKind GetKind() const = 0;
  [[nodiscard]] virtual uint32_t GetWidth() const = 0;
  [[nodiscard]] virtual uint32_t GetHeight() const = 0;
  virtual void PollEvents() = 0;
  [[nodiscard]] virtual bool ShouldClose() const = 0;
  [[nodiscard]] virtual bool IsMinimized() const { return false; }
  [[nodiscard]] virtual GLFWwindow *GetGlfwWindow() const { return nullptr; }
};
} // namespace Axiom
