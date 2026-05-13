#pragma once

#include "Session/SessionTypes.h"

#include <filesystem>
#include <glm/vec4.hpp>
#include <optional>
#include <string>

namespace Axiom::Assets {

constexpr uint32_t kCookedMaterialFormatVersion = 1;

struct CookedMaterialData {
  glm::vec4 BaseColorFactor{1.0f};
  float Metallic{0.0f};
  float Roughness{0.5f};
  std::string TextureAssetPath;
};

bool SaveCookedMaterialAsset(const std::filesystem::path &Path,
                             const CookedMaterialData &Material,
                             AssetId Asset);

std::optional<CookedMaterialData>
LoadCookedMaterialAsset(const std::filesystem::path &Path);

} // namespace Axiom::Assets
