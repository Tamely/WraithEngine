#include "AssetCooker.h"

#include "Assets/CookedMaterialAsset.h"
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

uint64_t HashString(std::string_view Text) {
  return static_cast<uint64_t>(std::hash<std::string_view>{}(Text));
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

std::filesystem::path BuildCookedMaterialRelativePath(
    const std::filesystem::path &RelativeAssetPath) {
  auto Cooked = BuildCookedRelativePath(RelativeAssetPath);
  Cooked.replace_extension(".wmat");
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

  const auto Scene = LoadBasicMeshAssetFromSource(SourcePath);
  if (!Scene.has_value()) {
    A_CORE_WARN("AssetCooker: failed to import source mesh '{}'",
                SourcePath.string());
    return std::nullopt;
  }

  MeshSceneData CookedScene = *Scene;
  std::vector<std::string> MaterialAssetPaths(CookedScene.Instances.size());
  const std::string AssetStem = RelativeAssetPath.stem().generic_string();
  for (size_t InstanceIndex = 0; InstanceIndex < CookedScene.Instances.size();
       ++InstanceIndex) {
    auto &Instance = CookedScene.Instances[InstanceIndex];
    if (!Instance.Material) {
      continue;
    }

    std::string TextureAssetPath;
    if (Instance.Material->BaseColorTexture &&
        Instance.Material->BaseColorTexture->IsValid()) {
      const std::filesystem::path RelativeTexturePath =
          std::filesystem::path("Generated/MeshTextures") /
          (AssetStem + "__" + std::to_string(InstanceIndex));
      const auto TextureEntry = CookTextureAsset(
          ContentRoot, RelativeTexturePath, *Instance.Material->BaseColorTexture);
      if (TextureEntry.has_value()) {
        TextureAssetPath = TextureEntry->RelativePath;
      }
    } else if (!Instance.Material->TextureAssetPath.empty()) {
      const auto TextureEntry =
          CookTextureAsset(ContentRoot, Instance.Material->TextureAssetPath);
      if (TextureEntry.has_value()) {
        TextureAssetPath = TextureEntry->RelativePath;
      }
    }

    const std::filesystem::path RelativeMaterialPath =
        std::filesystem::path("Generated/MeshMaterials") /
        (AssetStem + "__" + std::to_string(InstanceIndex));
    const auto MaterialEntry = CookMaterialAsset(
        ContentRoot, RelativeMaterialPath,
        {.BaseColorFactor = Instance.Material->BaseColorFactor,
         .Metallic = Instance.Material->Metallic,
         .Roughness = Instance.Material->Roughness,
         .TextureAssetPath = TextureAssetPath});
    if (MaterialEntry.has_value()) {
      MaterialAssetPaths[InstanceIndex] = MaterialEntry->RelativePath;
    }
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

  CookedMeshSceneData BinaryScene;
  BinaryScene.Instances.reserve(CookedScene.Instances.size());
  for (size_t InstanceIndex = 0; InstanceIndex < CookedScene.Instances.size();
       ++InstanceIndex) {
    const auto &Instance = CookedScene.Instances[InstanceIndex];
    BinaryScene.Instances.push_back({
        .Name = Instance.Name,
        .MaterialAssetPath = MaterialAssetPaths[InstanceIndex],
        .Mesh = Instance.Mesh,
        .Transform = Instance.Transform,
    });
  }

  if (!SaveCookedMeshAsset(CookedAbsolutePath, BinaryScene, Asset)) {
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

  return CookTextureAsset(ContentRoot, RelativeAssetPath, *Texture);
}

std::optional<AssetCookManifestEntry>
CookTextureAsset(const std::filesystem::path &ContentRoot,
                 const std::filesystem::path &RelativeTexturePath,
                 const TextureSourceData &Texture) {
  const AssetId Asset = AssetIdFromRelativePath(RelativeTexturePath);
  if (!Texture.IsValid()) {
    A_CORE_WARN("AssetCooker: invalid generated texture '{}'",
                RelativeTexturePath.string());
    return std::nullopt;
  }

  const std::filesystem::path CookedRelativePath =
      BuildCookedTextureRelativePath(RelativeTexturePath);
  const std::filesystem::path CookedAbsolutePath = ContentRoot / CookedRelativePath;
  std::error_code Ec;
  std::filesystem::create_directories(CookedAbsolutePath.parent_path(), Ec);
  if (Ec) {
    A_CORE_WARN("AssetCooker: failed to create cooked texture directory '{}': {}",
                CookedAbsolutePath.parent_path().string(), Ec.message());
    return std::nullopt;
  }

  if (!SaveCookedTextureAsset(CookedAbsolutePath, Texture, Asset)) {
    return std::nullopt;
  }

  const std::filesystem::path ManifestPath =
      ContentRoot / "Cooked" / "AssetCookManifest.json";
  AssetCookManifest Manifest =
      LoadAssetCookManifest(ManifestPath).value_or(AssetCookManifest{});

  const uint64_t SourceHash = HashString(std::to_string(Texture.Width) + "|" +
                                         std::to_string(Texture.Height) + "|" +
                                         std::to_string(Texture.Pixels.size()));

  AssetCookManifestEntry Entry{
      .Id = Asset,
      .Kind = AssetKind::Texture,
      .RelativePath = RelativeTexturePath.generic_string(),
      .CookedPath = CookedRelativePath.generic_string(),
      .FormatVersion = kCookedTextureFormatVersion,
      .SourceHash = SourceHash,
  };
  UpsertManifestEntry(Manifest, Entry);

  if (!SaveAssetCookManifest(ManifestPath, Manifest)) {
    return std::nullopt;
  }

  return Entry;
}

std::optional<AssetCookManifestEntry>
CookHDRTextureAsset(const std::filesystem::path &ContentRoot,
                    const std::filesystem::path &RelativeAssetPath) {
  const std::filesystem::path SourcePath = ContentRoot / RelativeAssetPath;
  const auto SourceHash = HashFileContents(SourcePath);
  if (!SourceHash.has_value()) {
    A_CORE_WARN("AssetCooker: failed to hash source HDR texture '{}'",
                SourcePath.string());
    return std::nullopt;
  }

  const auto Texture = LoadHDRTextureFromSourceFile(SourcePath);
  if (!Texture || !Texture->IsValid()) {
    A_CORE_WARN("AssetCooker: failed to decode source HDR texture '{}'",
                SourcePath.string());
    return std::nullopt;
  }

  return CookHDRTextureAsset(ContentRoot, RelativeAssetPath, *Texture);
}

std::optional<AssetCookManifestEntry>
CookHDRTextureAsset(const std::filesystem::path &ContentRoot,
                    const std::filesystem::path &RelativeTexturePath,
                    const HDRTextureSourceData &Texture) {
  const AssetId Asset = AssetIdFromRelativePath(RelativeTexturePath);
  if (!Texture.IsValid()) {
    A_CORE_WARN("AssetCooker: invalid generated HDR texture '{}'",
                RelativeTexturePath.string());
    return std::nullopt;
  }

  const std::filesystem::path CookedRelativePath =
      BuildCookedTextureRelativePath(RelativeTexturePath);
  const std::filesystem::path CookedAbsolutePath = ContentRoot / CookedRelativePath;
  std::error_code Ec;
  std::filesystem::create_directories(CookedAbsolutePath.parent_path(), Ec);
  if (Ec) {
    A_CORE_WARN("AssetCooker: failed to create cooked HDR texture directory '{}': {}",
                CookedAbsolutePath.parent_path().string(), Ec.message());
    return std::nullopt;
  }

  if (!SaveCookedHDRTextureAsset(CookedAbsolutePath, Texture, Asset)) {
    return std::nullopt;
  }

  const std::filesystem::path ManifestPath =
      ContentRoot / "Cooked" / "AssetCookManifest.json";
  AssetCookManifest Manifest =
      LoadAssetCookManifest(ManifestPath).value_or(AssetCookManifest{});

  const uint64_t SourceHash = HashString(
      std::to_string(Texture.Width) + "|" + std::to_string(Texture.Height) +
      "|hdr|" + std::to_string(Texture.Pixels.size()));

  AssetCookManifestEntry Entry{
      .Id = Asset,
      .Kind = AssetKind::Texture,
      .RelativePath = RelativeTexturePath.generic_string(),
      .CookedPath = CookedRelativePath.generic_string(),
      .FormatVersion = kCookedTextureFormatVersion,
      .SourceHash = SourceHash,
  };
  UpsertManifestEntry(Manifest, Entry);

  if (!SaveAssetCookManifest(ManifestPath, Manifest)) {
    return std::nullopt;
  }

  return Entry;
}

std::optional<AssetCookManifestEntry>
CookMaterialAsset(const std::filesystem::path &ContentRoot,
                  const std::filesystem::path &RelativeMaterialPath,
                  const CookedMaterialData &Material) {
  const AssetId Asset = AssetIdFromRelativePath(RelativeMaterialPath);
  const std::filesystem::path CookedRelativePath =
      BuildCookedMaterialRelativePath(RelativeMaterialPath);
  const std::filesystem::path CookedAbsolutePath = ContentRoot / CookedRelativePath;
  std::error_code Ec;
  std::filesystem::create_directories(CookedAbsolutePath.parent_path(), Ec);
  if (Ec) {
    A_CORE_WARN("AssetCooker: failed to create cooked material directory '{}': {}",
                CookedAbsolutePath.parent_path().string(), Ec.message());
    return std::nullopt;
  }

  if (!SaveCookedMaterialAsset(CookedAbsolutePath, Material, Asset)) {
    return std::nullopt;
  }

  const std::filesystem::path ManifestPath =
      ContentRoot / "Cooked" / "AssetCookManifest.json";
  AssetCookManifest Manifest =
      LoadAssetCookManifest(ManifestPath).value_or(AssetCookManifest{});

  const std::string HashInput =
      std::to_string(Material.BaseColorFactor.r) + "|" +
      std::to_string(Material.BaseColorFactor.g) + "|" +
      std::to_string(Material.BaseColorFactor.b) + "|" +
      std::to_string(Material.BaseColorFactor.a) + "|" +
      std::to_string(Material.Metallic) + "|" +
      std::to_string(Material.Roughness) + "|" + Material.TextureAssetPath;

  AssetCookManifestEntry Entry{
      .Id = Asset,
      .Kind = AssetKind::Material,
      .RelativePath = RelativeMaterialPath.generic_string(),
      .CookedPath = CookedRelativePath.generic_string(),
      .FormatVersion = kCookedMaterialFormatVersion,
      .SourceHash = HashString(HashInput),
  };
  UpsertManifestEntry(Manifest, Entry);

  if (!SaveAssetCookManifest(ManifestPath, Manifest)) {
    return std::nullopt;
  }

  return Entry;
}

} // namespace Axiom::Assets
