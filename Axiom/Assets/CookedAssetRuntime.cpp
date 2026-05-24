#include "CookedAssetRuntime.h"

#include "Assets/CookedMaterialAsset.h"
#include "Assets/CookedMeshAsset.h"
#include "Assets/CookedTextureAsset.h"
#include "Assets/IAssetSource.h"
#include "Assets/SceneFile.h"

#include <fstream>
#include <unordered_set>
#include <unordered_map>

namespace Axiom::Assets {
namespace {
bool ReadPackageManifestFields(
    const std::filesystem::path &ManifestPath,
    std::unordered_map<std::string, std::string> &Fields) {
  std::ifstream File(ManifestPath);
  if (!File.is_open()) {
    return false;
  }

  const std::string Text((std::istreambuf_iterator<char>(File)),
                         std::istreambuf_iterator<char>());
  std::size_t Position = 0;
  auto SkipWs = [&]() {
    while (Position < Text.size() &&
           (Text[Position] == ' ' || Text[Position] == '\n' ||
            Text[Position] == '\r' || Text[Position] == '\t')) {
      ++Position;
    }
  };
  auto ParseString = [&]() -> std::optional<std::string> {
    SkipWs();
    if (Position >= Text.size() || Text[Position] != '"') {
      return std::nullopt;
    }
    ++Position;
    std::string Result;
    while (Position < Text.size()) {
      const char Character = Text[Position++];
      if (Character == '"') {
        return Result;
      }
      if (Character == '\\' && Position < Text.size()) {
        Result.push_back(Text[Position++]);
      } else {
        Result.push_back(Character);
      }
    }
    return std::nullopt;
  };

  SkipWs();
  if (Position >= Text.size() || Text[Position] != '{') {
    return false;
  }
  ++Position;
  while (true) {
    SkipWs();
    if (Position >= Text.size()) {
      return false;
    }
    if (Text[Position] == '}') {
      return true;
    }

    const auto Key = ParseString();
    if (!Key.has_value()) {
      return false;
    }
    SkipWs();
    if (Position >= Text.size() || Text[Position] != ':') {
      return false;
    }
    ++Position;
    const auto Value = ParseString();
    if (Value.has_value()) {
      Fields[*Key] = *Value;
    } else {
      SkipWs();
      const std::size_t ValueStart = Position;
      while (Position < Text.size() && Text[Position] != ',' && Text[Position] != '}') {
        ++Position;
      }
      Fields[*Key] = Text.substr(ValueStart, Position - ValueStart);
    }

    SkipWs();
    if (Position < Text.size() && Text[Position] == ',') {
      ++Position;
      continue;
    }
    if (Position < Text.size() && Text[Position] == '}') {
      return true;
    }
  }
}
} // namespace

bool IsCookedOnlyContentPath(const std::filesystem::path &Path) {
  const auto ContentRoot = FindContentRootForPath(Path);
  if (!ContentRoot.has_value()) {
    return false;
  }

  const auto PackageManifestPath = ContentRoot->parent_path() / "package.wraith.json";
  return std::filesystem::exists(PackageManifestPath);
}

std::optional<PackagedContentDescriptor>
ResolvePackagedContentDescriptor(const std::filesystem::path &Path,
                                std::string *FailureReason) {
  const auto ContentRoot = FindContentRootForPath(Path);
  if (!ContentRoot.has_value()) {
    if (FailureReason != nullptr) {
      *FailureReason = "Path is not inside a packaged Content directory.";
    }
    return std::nullopt;
  }

  const auto PackageRoot = ContentRoot->parent_path();
  const auto PackageManifestPath = PackageRoot / "package.wraith.json";
  if (!std::filesystem::exists(PackageManifestPath)) {
    if (FailureReason != nullptr) {
      *FailureReason = "package.wraith.json is missing.";
    }
    return std::nullopt;
  }

  std::unordered_map<std::string, std::string> Fields;
  if (!ReadPackageManifestFields(PackageManifestPath, Fields)) {
    if (FailureReason != nullptr) {
      *FailureReason = "package.wraith.json could not be parsed.";
    }
    return std::nullopt;
  }

  const auto ContentModeIt = Fields.find("contentMode");
  const auto SceneAssetIt = Fields.find("sceneAsset");
  const auto CookManifestIt = Fields.find("assetCookManifest");
  const auto EngineContentIt = Fields.find("engineContentDir");
  if (ContentModeIt == Fields.end() || SceneAssetIt == Fields.end() ||
      CookManifestIt == Fields.end() || EngineContentIt == Fields.end()) {
    if (FailureReason != nullptr) {
      *FailureReason = "package.wraith.json is missing required packaged runtime fields.";
    }
    return std::nullopt;
  }
  if (ContentModeIt->second != "cooked-only-v1") {
    if (FailureReason != nullptr) {
      *FailureReason = "package.wraith.json contentMode is not cooked-only-v1.";
    }
    return std::nullopt;
  }

  return PackagedContentDescriptor{
      .PackageRoot = PackageRoot,
      .ContentRoot = *ContentRoot,
      .SceneAssetPath = PackageRoot / SceneAssetIt->second,
      .CookManifestPath = PackageRoot / CookManifestIt->second,
      .EngineContentDir = PackageRoot / EngineContentIt->second,
  };
}

bool ValidatePackagedContentDescriptor(const PackagedContentDescriptor &Descriptor,
                                       std::string *FailureReason) {
  if (!std::filesystem::exists(Descriptor.ContentRoot) ||
      !std::filesystem::is_directory(Descriptor.ContentRoot)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Expected package root to contain a Content directory at '" +
                       Descriptor.ContentRoot.string() + "'.";
    }
    return false;
  }
  if (!std::filesystem::exists(Descriptor.SceneAssetPath)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Packaged scene asset is missing at '" +
                       Descriptor.SceneAssetPath.string() + "'.";
    }
    return false;
  }
  if (!std::filesystem::exists(Descriptor.CookManifestPath)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Packaged asset cook manifest is missing at '" +
                       Descriptor.CookManifestPath.string() + "'.";
    }
    return false;
  }
  if (!std::filesystem::exists(Descriptor.EngineContentDir) ||
      !std::filesystem::is_directory(Descriptor.EngineContentDir)) {
    if (FailureReason != nullptr) {
      *FailureReason = "Packaged engine content directory is missing at '" +
                       Descriptor.EngineContentDir.string() + "'.";
    }
    return false;
  }

  const auto LoadedScene = LoadCookedSceneFromFile(Descriptor.SceneAssetPath);
  if (!LoadedScene.has_value()) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to load packaged scene asset '" +
                       Descriptor.SceneAssetPath.string() + "'.";
    }
    return false;
  }

  const CookedAssetSource CookedSource(Descriptor.ContentRoot);
  if (!CookedSource.HasManifest()) {
    if (FailureReason != nullptr) {
      *FailureReason = "Packaged asset cook manifest could not be loaded from '" +
                       Descriptor.CookManifestPath.string() + "'.";
    }
    return false;
  }

  auto ValidateResolvedAssetPath = [&](std::string_view RelativePath,
                                       std::string_view Usage) -> bool {
    if (RelativePath.empty()) {
      return true;
    }

    const std::filesystem::path RelativeAssetPath(RelativePath);
    if (RelativeAssetPath.is_absolute()) {
      if (FailureReason != nullptr) {
        *FailureReason = std::string(Usage) + " '" + std::string(RelativePath) +
                         "' must be content-relative, not absolute.";
      }
      return false;
    }

    const auto Begin = RelativeAssetPath.begin();
    const bool IsEngineRelative =
        Begin != RelativeAssetPath.end() && Begin->string() == "Engine";
    if (IsEngineRelative) {
      const auto EngineAssetPath = Descriptor.ContentRoot / RelativeAssetPath;
      if (!std::filesystem::exists(EngineAssetPath)) {
        if (FailureReason != nullptr) {
          *FailureReason = std::string(Usage) + " '" +
                           RelativeAssetPath.generic_string() +
                           "' does not resolve inside packaged engine content.";
        }
        return false;
      }
      return true;
    }

    const auto CookedPath =
        CookedSource.Resolve(AssetIdFromRelativePath(RelativeAssetPath));
    if (!CookedPath.has_value()) {
      if (FailureReason != nullptr) {
        *FailureReason = std::string(Usage) + " '" +
                         RelativeAssetPath.generic_string() +
                         "' is not present in the packaged asset cook manifest.";
      }
      return false;
    }
    if (!std::filesystem::exists(*CookedPath)) {
      if (FailureReason != nullptr) {
        *FailureReason = std::string(Usage) + " '" +
                         RelativeAssetPath.generic_string() +
                         "' maps to missing cooked asset '" +
                         CookedPath->string() + "'.";
      }
      return false;
    }
    return true;
  };

  std::unordered_set<std::string> MeshPaths;
  std::unordered_set<std::string> TexturePaths;
  for (const auto &Instance : LoadedScene->MeshInstances) {
    if (!Instance.AssetRelativePath.empty()) {
      MeshPaths.insert(Instance.AssetRelativePath);
    }
    if (Instance.Material != nullptr &&
        !Instance.Material->TextureAssetPath.empty()) {
      TexturePaths.insert(Instance.Material->TextureAssetPath);
    }
  }
  for (const auto &[ObjectId, Details] : LoadedScene->ObjectDetailsById) {
    static_cast<void>(ObjectId);
    if (!Details.AssetRelativePath.empty()) {
      MeshPaths.insert(Details.AssetRelativePath);
    }
    if (Details.Material.has_value() &&
        Details.Material->TextureAssetPath.has_value() &&
        !Details.Material->TextureAssetPath->empty()) {
      TexturePaths.insert(*Details.Material->TextureAssetPath);
    }
  }
  if (!LoadedScene->WorldSettings.SkyboxHDRPath.empty()) {
    TexturePaths.insert(LoadedScene->WorldSettings.SkyboxHDRPath);
  }

  for (const std::string &MeshPath : MeshPaths) {
    if (!ValidateResolvedAssetPath(MeshPath, "Mesh asset reference")) {
      return false;
    }
  }
  for (const std::string &TexturePath : TexturePaths) {
    if (!ValidateResolvedAssetPath(TexturePath, "Texture asset reference")) {
      return false;
    }
  }

  return true;
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
