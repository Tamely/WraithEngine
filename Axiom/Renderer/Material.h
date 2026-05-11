#pragma once

#include <cstddef>
#include <cstdint>
#include <glm/vec4.hpp>
#include <memory>
#include <string>
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
  glm::vec4 BaseColorFactor{1.0f};
  float Metallic{0.0f};
  float Roughness{0.5f};
  // Content-relative path of the standalone texture assigned via
  // SetMaterialTextureCommand; empty if the texture came from the mesh asset.
  std::string TextureAssetPath;
};

using MaterialInstanceRef = std::shared_ptr<MaterialInstance>;
} // namespace Axiom
