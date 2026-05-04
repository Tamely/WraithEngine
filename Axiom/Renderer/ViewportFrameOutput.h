#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Axiom {
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
};

class IViewportFrameOutput {
public:
  virtual ~IViewportFrameOutput() = default;
  virtual void OnViewportFrame(const ViewportFrame &Frame) = 0;
};
} // namespace Axiom
