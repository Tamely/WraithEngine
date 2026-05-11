#pragma once

#include "IAssetImporter.h"

namespace Axiom::Assets {

class AssimpImporter final : public IAssetImporter {
public:
  std::optional<MeshSceneData> Import(const std::filesystem::path &Path) override;
};

} // namespace Axiom::Assets
