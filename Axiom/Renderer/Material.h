#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Axiom {
struct TextureSourceData {
  uint32_t Width{0};
  uint32_t Height{0};
  std::vector<std::uint8_t> Pixels;

  [[nodiscard]] bool IsValid() const {
    return Width > 0 && Height > 0 &&
           Pixels.size() == static_cast<size_t>(Width) *
                                static_cast<size_t>(Height) * 4;
  }
};

using TextureSourceDataRef = std::shared_ptr<TextureSourceData>;

struct MaterialInstance {
  TextureSourceDataRef BaseColorTexture;
};

using MaterialInstanceRef = std::shared_ptr<MaterialInstance>;
} // namespace Axiom
