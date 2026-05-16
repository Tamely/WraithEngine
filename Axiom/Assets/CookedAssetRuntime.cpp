#include "CookedAssetRuntime.h"

#include "Assets/CookedMaterialAsset.h"
#include "Assets/CookedMeshAsset.h"
#include "Assets/CookedTextureAsset.h"
#include "Assets/IAssetSource.h"

namespace Axiom::Assets {

bool IsCookedOnlyContentPath(const std::filesystem::path &Path) {
  const auto ContentRoot = FindContentRootForPath(Path);
  if (!ContentRoot.has_value()) {
    return false;
  }

  const auto PackageManifestPath = ContentRoot->parent_path() / "package.wraith.json";
  return std::filesystem::exists(PackageManifestPath);
}

std::optional<std::filesystem::path>
FindContentRootForPath(const std::filesystem::path &Path) {
  if (Path.empty()) {
    return std::nullopt;
  }

  std::filesystem::path Current =
      std::filesystem::is_directory(Path) ? Path : Path.parent_path();
  while (!Current.empty()) {
    if (Current.filename() == "Content") {
      return Current;
    }
    const auto Parent = Current.parent_path();
    if (Parent == Current) {
      break;
    }
    Current = Parent;
  }

  return std::nullopt;
}

std::optional<MeshSceneData>
LoadCookedMeshAssetIfAvailable(const std::filesystem::path &Path) {
  const auto ContentRoot = FindContentRootForPath(Path);
  if (!ContentRoot.has_value()) {
    return std::nullopt;
  }

  std::error_code Ec;
  const auto RelativePath = std::filesystem::relative(Path, *ContentRoot, Ec);
  if (Ec) {
    return std::nullopt;
  }

  const CookedAssetSource CookedSource(*ContentRoot);
  if (!CookedSource.HasManifest()) {
    return std::nullopt;
  }

  const auto CookedPath =
      CookedSource.Resolve(AssetIdFromRelativePath(RelativePath));
  if (!CookedPath.has_value()) {
    return std::nullopt;
  }

  const auto CookedScene = LoadCookedMeshAsset(*CookedPath);
  if (!CookedScene.has_value()) {
    return std::nullopt;
  }

  return ToRuntimeMeshSceneData(*CookedScene, *ContentRoot);
}

TextureSourceDataRef
LoadCookedTextureAssetIfAvailable(const std::filesystem::path &Path) {
  const auto ContentRoot = FindContentRootForPath(Path);
  if (!ContentRoot.has_value()) {
    return nullptr;
  }

  std::error_code Ec;
  const auto RelativePath = std::filesystem::relative(Path, *ContentRoot, Ec);
  if (Ec) {
    return nullptr;
  }

  const CookedAssetSource CookedSource(*ContentRoot);
  if (!CookedSource.HasManifest()) {
    return nullptr;
  }

  const auto CookedPath =
      CookedSource.Resolve(AssetIdFromRelativePath(RelativePath));
  if (!CookedPath.has_value()) {
    return nullptr;
  }

  const auto CookedTexture = LoadCookedTextureAsset(*CookedPath);
  if (!CookedTexture.has_value()) {
    return nullptr;
  }

  return std::make_shared<TextureSourceData>(*CookedTexture);
}

HDRTextureSourceDataRef
LoadCookedHDRTextureAssetIfAvailable(const std::filesystem::path &Path) {
  const auto ContentRoot = FindContentRootForPath(Path);
  if (!ContentRoot.has_value()) {
    return nullptr;
  }

  std::error_code Ec;
  const auto RelativePath = std::filesystem::relative(Path, *ContentRoot, Ec);
  if (Ec) {
    return nullptr;
  }

  const CookedAssetSource CookedSource(*ContentRoot);
  if (!CookedSource.HasManifest()) {
    return nullptr;
  }

  const auto CookedPath =
      CookedSource.Resolve(AssetIdFromRelativePath(RelativePath));
  if (!CookedPath.has_value()) {
    return nullptr;
  }

  const auto CookedTexture = LoadCookedHDRTextureAsset(*CookedPath);
  if (!CookedTexture.has_value()) {
    return nullptr;
  }

  return std::make_shared<HDRTextureSourceData>(*CookedTexture);
}

std::optional<CookedMaterialData>
LoadCookedMaterialAssetIfAvailable(const std::filesystem::path &Path) {
  const auto ContentRoot = FindContentRootForPath(Path);
  if (!ContentRoot.has_value()) {
    return std::nullopt;
  }

  std::error_code Ec;
  const auto RelativePath = std::filesystem::relative(Path, *ContentRoot, Ec);
  if (Ec) {
    return std::nullopt;
  }

  const CookedAssetSource CookedSource(*ContentRoot);
  if (!CookedSource.HasManifest()) {
    return std::nullopt;
  }

  const auto CookedPath =
      CookedSource.Resolve(AssetIdFromRelativePath(RelativePath));
  if (!CookedPath.has_value()) {
    return std::nullopt;
  }

  return LoadCookedMaterialAsset(*CookedPath);
}

} // namespace Axiom::Assets
