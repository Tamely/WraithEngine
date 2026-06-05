#include "CookedAssetRuntime.h"

#include "Assets/CookedMaterialAsset.h"
#include "Assets/CookedMeshAsset.h"
#include "Assets/CookedTextureAsset.h"
#include "Assets/IAssetSource.h"
#include "Assets/SceneFile.h"

#include <rapidjson/document.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Axiom::Assets {
namespace {
constexpr char kCookedSceneMagic[] = {'W', 'S', 'C', 'N'};
constexpr std::uint32_t kCookedSceneVersion = 1;

struct CookedSceneAssetReferences {
  std::vector<std::string> MeshAssetPaths;
  std::vector<std::string> MaterialAssetPaths;
  std::vector<std::string> TextureAssetPaths;
};

bool ReadPackageManifestFields(
    const std::filesystem::path &ManifestPath,
    std::unordered_map<std::string, std::string> &Fields) {
  std::ifstream File(ManifestPath);
  if (!File.is_open()) {
    return false;
  }

  std::string Text((std::istreambuf_iterator<char>(File)),
                   std::istreambuf_iterator<char>());
  rapidjson::Document Document;
  Document.ParseInsitu<rapidjson::kParseStopWhenDoneFlag>(Text.data());
  if (Document.HasParseError() || !Document.IsObject()) {
    return false;
  }

  for (const auto &Member : Document.GetObject()) {
    if (Member.value.IsString()) {
      Fields.emplace(
          Member.name.GetString(),
          std::string(Member.value.GetString(), Member.value.GetStringLength()));
      continue;
    }

    if (Member.value.IsBool()) {
      Fields.emplace(Member.name.GetString(),
                     Member.value.GetBool() ? "true" : "false");
      continue;
    }

    if (Member.value.IsInt64()) {
      Fields.emplace(Member.name.GetString(),
                     std::to_string(Member.value.GetInt64()));
      continue;
    }

    if (Member.value.IsUint64()) {
      Fields.emplace(Member.name.GetString(),
                     std::to_string(Member.value.GetUint64()));
      continue;
    }

    if (Member.value.IsDouble()) {
      Fields.emplace(Member.name.GetString(),
                     std::to_string(Member.value.GetDouble()));
      continue;
    }

    if (Member.value.IsNull()) {
      Fields.emplace(Member.name.GetString(), "null");
    }
  }

  return true;
}

bool ReadCookedSceneAssetReferences(const std::filesystem::path &ScenePath,
                                    CookedSceneAssetReferences &References) {
  std::ifstream File(ScenePath, std::ios::binary);
  if (!File.is_open()) {
    return false;
  }

  char Magic[sizeof(kCookedSceneMagic)];
  File.read(Magic, sizeof(Magic));
  if (!File.good() || std::memcmp(Magic, kCookedSceneMagic, sizeof(Magic)) != 0) {
    return false;
  }

  std::uint32_t Version = 0;
  std::uint64_t PayloadSize = 0;
  File.read(reinterpret_cast<char *>(&Version), sizeof(Version));
  File.read(reinterpret_cast<char *>(&PayloadSize), sizeof(PayloadSize));
  if (!File.good() || Version != kCookedSceneVersion) {
    return false;
  }

  std::string Payload(PayloadSize, '\0');
  File.read(Payload.data(), static_cast<std::streamsize>(PayloadSize));
  if (!File.good()) {
    return false;
  }

  rapidjson::Document Document;
  Document.ParseInsitu<rapidjson::kParseStopWhenDoneFlag>(Payload.data());
  if (Document.HasParseError() || !Document.IsObject()) {
    return false;
  }

  const auto ObjectsIt = Document.FindMember("objects");
  if (ObjectsIt == Document.MemberEnd() || !ObjectsIt->value.IsArray()) {
    return true;
  }

  for (const auto &ObjectValue : ObjectsIt->value.GetArray()) {
    if (!ObjectValue.IsObject()) {
      continue;
    }

    const auto AssetRelativePathIt = ObjectValue.FindMember("assetRelativePath");
    if (AssetRelativePathIt != ObjectValue.MemberEnd() &&
        AssetRelativePathIt->value.IsString()) {
      References.MeshAssetPaths.emplace_back(
          AssetRelativePathIt->value.GetString(),
          AssetRelativePathIt->value.GetStringLength());
    }

    const auto MaterialAssetPathIt =
        ObjectValue.FindMember("materialAssetPath");
    if (MaterialAssetPathIt != ObjectValue.MemberEnd() &&
        MaterialAssetPathIt->value.IsString()) {
      References.MaterialAssetPaths.emplace_back(
          MaterialAssetPathIt->value.GetString(),
          MaterialAssetPathIt->value.GetStringLength());
    }

    const auto TextureAssetPathIt = ObjectValue.FindMember("textureAssetPath");
    if (TextureAssetPathIt != ObjectValue.MemberEnd() &&
        TextureAssetPathIt->value.IsString()) {
      References.TextureAssetPaths.emplace_back(
          TextureAssetPathIt->value.GetString(),
          TextureAssetPathIt->value.GetStringLength());
    }
  }

  return true;
}
} // namespace

bool IsCookedOnlyContentPath(const std::filesystem::path &Path) {
  const auto ContentRoot = FindContentRootForPath(Path);
  if (!ContentRoot.has_value()) {
    return false;
  }

  const auto PackageManifestPath =
      ContentRoot->parent_path() / "package.wraith.json";
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
      *FailureReason =
          "package.wraith.json is missing required packaged runtime fields.";
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

  CookedSceneAssetReferences SceneReferences;
  if (!ReadCookedSceneAssetReferences(Descriptor.SceneAssetPath,
                                      SceneReferences)) {
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
                         "' resolves to missing cooked asset '" +
                         CookedPath->string() + "'.";
      }
      return false;
    }

    return true;
  };

  for (const std::string &MeshAssetPath : SceneReferences.MeshAssetPaths) {
    if (!ValidateResolvedAssetPath(MeshAssetPath, "Mesh asset")) {
      return false;
    }
  }

  for (const std::string &MaterialAssetPath :
       SceneReferences.MaterialAssetPaths) {
    if (!ValidateResolvedAssetPath(MaterialAssetPath, "Material asset")) {
      return false;
    }
  }

  for (const std::string &TextureAssetPath : SceneReferences.TextureAssetPaths) {
    if (!ValidateResolvedAssetPath(TextureAssetPath, "Texture asset")) {
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
