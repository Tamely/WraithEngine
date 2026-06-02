#pragma once

#include <cstddef>
#include <cstdint>
#include <compare>
#include <functional>
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

struct HDRTextureSourceData {
  uint32_t Width{0};
  uint32_t Height{0};
  std::vector<float> Pixels; // RGBA floats, 4 per pixel

  [[nodiscard]] bool IsValid() const {
    return Width > 0 && Height > 0 &&
           Pixels.size() == static_cast<size_t>(Width) *
                                static_cast<size_t>(Height) * 4;
  }
};

using HDRTextureSourceDataRef = std::shared_ptr<HDRTextureSourceData>;

struct MaterialHandle {
  uint32_t Value{0};

  [[nodiscard]] bool IsValid() const { return Value != 0; }
  auto operator<=>(const MaterialHandle &) const = default;
};

struct MaterialHandleHash {
  size_t operator()(const MaterialHandle &Handle) const noexcept {
    return std::hash<uint32_t>{}(Handle.Value);
  }
};

struct MaterialInstance {
  TextureSourceDataRef BaseColorTexture;
  glm::vec4 BaseColorFactor{1.0f};
  float Metallic{0.0f};
  float Roughness{0.5f};
  uint64_t Revision{0};
  // Content-relative path of the standalone texture assigned via
  // SetMaterialTextureCommand; empty if the texture came from the mesh asset.
  std::string TextureAssetPath;
};
inline void MarkMaterialInstanceDirty(MaterialInstance &Material) {
  ++Material.Revision;
}
} // namespace Axiom
