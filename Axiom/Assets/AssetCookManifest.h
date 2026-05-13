#pragma once

#include "Assets/IAssetSource.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Axiom::Assets {

struct AssetCookManifestEntry {
  AssetId Id;
  AssetKind Kind{AssetKind::Unknown};
  std::string RelativePath;
  std::string CookedPath;
  uint32_t FormatVersion{0};
  uint64_t SourceHash{0};
};

// Stores cooked-asset lookup metadata used by runtime asset sources.
struct AssetCookManifest {
  std::vector<AssetCookManifestEntry> Entries;
};

std::optional<AssetCookManifest>
LoadAssetCookManifest(const std::filesystem::path &Path);

bool SaveAssetCookManifest(const std::filesystem::path &Path,
                           const AssetCookManifest &Manifest);

} // namespace Axiom::Assets
