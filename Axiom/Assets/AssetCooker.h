#pragma once

#include "Assets/AssetCookManifest.h"
#include "Assets/CookedMaterialAsset.h"
#include "Renderer/Material.h"

#include <filesystem>
#include <optional>

namespace Axiom::Assets {

// Imports a source mesh asset from the content directory, writes a cooked
// `.wmesh`, and updates the cook manifest entry for the asset.
std::optional<AssetCookManifestEntry>
CookMeshAsset(const std::filesystem::path &ContentRoot,
              const std::filesystem::path &RelativeAssetPath);

// Decodes a source texture asset from the content directory, writes a cooked
// `.wtex`, and updates the cook manifest entry for the asset.
std::optional<AssetCookManifestEntry>
CookTextureAsset(const std::filesystem::path &ContentRoot,
                 const std::filesystem::path &RelativeAssetPath);

// Writes generated cooked texture state under Content/Cooked and updates the
// cook manifest for the provided logical texture path.
std::optional<AssetCookManifestEntry>
CookTextureAsset(const std::filesystem::path &ContentRoot,
                 const std::filesystem::path &RelativeTexturePath,
                 const TextureSourceData &Texture);

// Writes generated cooked material state under Content/Cooked and updates the
// cook manifest for the provided logical material path.
std::optional<AssetCookManifestEntry>
CookMaterialAsset(const std::filesystem::path &ContentRoot,
                  const std::filesystem::path &RelativeMaterialPath,
                  const CookedMaterialData &Material);

} // namespace Axiom::Assets
