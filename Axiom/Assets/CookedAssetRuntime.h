#pragma once

#include "Assets/CookedMaterialAsset.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"

#include <filesystem>
#include <optional>
#include <string>

namespace Axiom::Assets {

struct PackagedContentDescriptor {
  std::filesystem::path PackageRoot;
  std::filesystem::path ContentRoot;
  std::filesystem::path SceneAssetPath;
  std::filesystem::path CookManifestPath;
  std::filesystem::path EngineContentDir;
};

std::optional<std::filesystem::path>
FindContentRootForPath(const std::filesystem::path &Path);

bool IsCookedOnlyContentPath(const std::filesystem::path &Path);

std::optional<PackagedContentDescriptor>
ResolvePackagedContentDescriptor(const std::filesystem::path &Path,
                                std::string *FailureReason = nullptr);

bool ValidatePackagedContentDescriptor(const PackagedContentDescriptor &Descriptor,
                                       std::string *FailureReason = nullptr);

std::optional<MeshSceneData>
LoadCookedMeshAssetIfAvailable(const std::filesystem::path &Path);

TextureSourceDataRef
LoadCookedTextureAssetIfAvailable(const std::filesystem::path &Path);

HDRTextureSourceDataRef
LoadCookedHDRTextureAssetIfAvailable(const std::filesystem::path &Path);

std::optional<CookedMaterialData>
LoadCookedMaterialAssetIfAvailable(const std::filesystem::path &Path);

} // namespace Axiom::Assets
