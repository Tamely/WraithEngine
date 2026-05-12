#include "AssetCooker.h"

#include "Assets/CookedMeshAsset.h"
#include "Assets/CookedTextureAsset.h"
#include "Assets/IAssetSource.h"
#include "Assets/MeshAsset.h"
#include "Core/Log.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace Axiom::Assets {
namespace {

uint64_t HashBytes(const std::vector<char> &Bytes) {
  return static_cast<uint64_t>(
      std::hash<std::string_view>{}(std::string_view(Bytes.data(), Bytes.size())));
}

std::optional<uint64_t> HashFileContents(const std::filesystem::path &Path) {
  std::ifstream Stream(Path, std::ios::binary);
  if (!Stream.is_open()) {
    return std::nullopt;
  }

  const std::vector<char> Bytes((std::istreambuf_iterator<char>(Stream)),
                                std::istreambuf_iterator<char>());
  return HashBytes(Bytes);
}

std::filesystem::path BuildCookedRelativePath(
    const std::filesystem::path &RelativeAssetPath) {
  std::filesystem::path Cooked = std::filesystem::path("Cooked") / RelativeAssetPath;
  return Cooked;
}

std::filesystem::path BuildCookedMeshRelativePath(
    const std::filesystem::path &RelativeAssetPath) {
  auto Cooked = BuildCookedRelativePath(RelativeAssetPath);
  Cooked.replace_extension(".wmesh");
  return Cooked;
}

std::filesystem::path BuildCookedTextureRelativePath(
    const std::filesystem::path &RelativeAssetPath) {
  auto Cooked = BuildCookedRelativePath(RelativeAssetPath);
  Cooked.replace_extension(".wtex");
  return Cooked;
}

void UpsertManifestEntry(AssetCookManifest &Manifest,
                         AssetCookManifestEntry Entry) {
  const auto It = std::find_if(
      Manifest.Entries.begin(), Manifest.Entries.end(),
      [&](const AssetCookManifestEntry &Existing) {
        return Existing.Id.Value == Entry.Id.Value;
      });
  if (It != Manifest.Entries.end()) {
    *It = std::move(Entry);
    return;
  }
  Manifest.Entries.push_back(std::move(Entry));
}

} // namespace

std::optional<AssetCookManifestEntry>
CookMeshAsset(const std::filesystem::path &ContentRoot,
              const std::filesystem::path &RelativeAssetPath) {
  const AssetId Asset = AssetIdFromRelativePath(RelativeAssetPath);
  const std::filesystem::path SourcePath = ContentRoot / RelativeAssetPath;
  const auto SourceHash = HashFileContents(SourcePath);
  if (!SourceHash.has_value()) {
    A_CORE_WARN("AssetCooker: failed to hash source asset '{}'",
                SourcePath.string());
    return std::nullopt;
  }

  const auto Scene = LoadBasicMeshAsset(SourcePath);
  if (!Scene.has_value()) {
    A_CORE_WARN("AssetCooker: failed to import source mesh '{}'",
                SourcePath.string());
    return std::nullopt;
  }

  const std::filesystem::path CookedRelativePath =
      BuildCookedMeshRelativePath(RelativeAssetPath);
  const std::filesystem::path CookedAbsolutePath = ContentRoot / CookedRelativePath;
  std::error_code Ec;
  std::filesystem::create_directories(CookedAbsolutePath.parent_path(), Ec);
  if (Ec) {
    A_CORE_WARN("AssetCooker: failed to create cooked asset directory '{}': {}",
                CookedAbsolutePath.parent_path().string(), Ec.message());
    return std::nullopt;
  }

  if (!SaveCookedMeshAsset(CookedAbsolutePath, ToCookedMeshSceneData(*Scene),
                           Asset)) {
    return std::nullopt;
  }

  const std::filesystem::path ManifestPath =
      ContentRoot / "Cooked" / "AssetCookManifest.json";
  AssetCookManifest Manifest =
      LoadAssetCookManifest(ManifestPath).value_or(AssetCookManifest{});

  AssetCookManifestEntry Entry{
      .Id = Asset,
      .Kind = AssetKind::Mesh,
      .RelativePath = RelativeAssetPath.generic_string(),
      .CookedPath = CookedRelativePath.generic_string(),
      .FormatVersion = kCookedMeshFormatVersion,
      .SourceHash = *SourceHash,
  };
  UpsertManifestEntry(Manifest, Entry);

  if (!SaveAssetCookManifest(ManifestPath, Manifest)) {
    return std::nullopt;
  }

  return Entry;
}

std::optional<AssetCookManifestEntry>
CookTextureAsset(const std::filesystem::path &ContentRoot,
                 const std::filesystem::path &RelativeAssetPath) {
  const AssetId Asset = AssetIdFromRelativePath(RelativeAssetPath);
  const std::filesystem::path SourcePath = ContentRoot / RelativeAssetPath;
  const auto SourceHash = HashFileContents(SourcePath);
  if (!SourceHash.has_value()) {
    A_CORE_WARN("AssetCooker: failed to hash source texture '{}'",
                SourcePath.string());
    return std::nullopt;
  }

  const auto Texture = LoadTextureFromFile(SourcePath);
  if (!Texture || !Texture->IsValid()) {
    A_CORE_WARN("AssetCooker: failed to decode source texture '{}'",
                SourcePath.string());
    return std::nullopt;
  }

  const std::filesystem::path CookedRelativePath =
      BuildCookedTextureRelativePath(RelativeAssetPath);
  const std::filesystem::path CookedAbsolutePath = ContentRoot / CookedRelativePath;
  std::error_code Ec;
  std::filesystem::create_directories(CookedAbsolutePath.parent_path(), Ec);
  if (Ec) {
    A_CORE_WARN("AssetCooker: failed to create cooked texture directory '{}': {}",
                CookedAbsolutePath.parent_path().string(), Ec.message());
    return std::nullopt;
  }

  if (!SaveCookedTextureAsset(CookedAbsolutePath, *Texture, Asset)) {
    return std::nullopt;
  }

  const std::filesystem::path ManifestPath =
      ContentRoot / "Cooked" / "AssetCookManifest.json";
  AssetCookManifest Manifest =
      LoadAssetCookManifest(ManifestPath).value_or(AssetCookManifest{});

  AssetCookManifestEntry Entry{
      .Id = Asset,
      .Kind = AssetKind::Texture,
      .RelativePath = RelativeAssetPath.generic_string(),
      .CookedPath = CookedRelativePath.generic_string(),
      .FormatVersion = kCookedTextureFormatVersion,
      .SourceHash = *SourceHash,
  };
  UpsertManifestEntry(Manifest, Entry);

  if (!SaveAssetCookManifest(ManifestPath, Manifest)) {
    return std::nullopt;
  }

  return Entry;
}

} // namespace Axiom::Assets
