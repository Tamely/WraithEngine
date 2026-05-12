#pragma once

#include "Assets/AssetCookManifest.h"

#include <filesystem>
#include <optional>

namespace Axiom::Assets {

// Imports a source mesh asset from the content directory, writes a cooked
// `.wmesh`, and updates the cook manifest entry for the asset.
std::optional<AssetCookManifestEntry>
CookMeshAsset(const std::filesystem::path &ContentRoot,
              const std::filesystem::path &RelativeAssetPath);

} // namespace Axiom::Assets
