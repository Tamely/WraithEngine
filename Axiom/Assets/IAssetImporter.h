#pragma once

#include "Renderer/Mesh.h"

#include <filesystem>
#include <optional>

namespace Axiom::Assets {

class IAssetImporter {
public:
  virtual ~IAssetImporter() = default;
  virtual std::optional<MeshSceneData> Import(const std::filesystem::path &Path) = 0;
};

} // namespace Axiom::Assets
