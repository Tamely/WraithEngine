#include "CookedMaterialAsset.h"

#include "Core/Log.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <type_traits>

namespace Axiom::Assets {
namespace {

constexpr std::array<char, 4> kMagic = {'W', 'M', 'A', 'T'};

struct FileHeader {
  char Magic[4];
  uint32_t Version;
  uint64_t AssetIdValue;
  std::array<float, 4> BaseColorFactor;
  float Metallic;
  float Roughness;
  uint32_t TexturePathLength;
};

static_assert(std::is_trivially_copyable_v<FileHeader>);

template <typename T>
bool WriteValue(std::ofstream &Stream, const T &Value) {
  Stream.write(reinterpret_cast<const char *>(&Value), sizeof(T));
  return Stream.good();
}

template <typename T> bool ReadValue(std::ifstream &Stream, T &Value) {
  Stream.read(reinterpret_cast<char *>(&Value), sizeof(T));
  return Stream.good();
}

} // namespace

bool SaveCookedMaterialAsset(const std::filesystem::path &Path,
                             const CookedMaterialData &Material,
                             AssetId Asset) {
  std::ofstream Stream(Path, std::ios::binary);
  if (!Stream.is_open()) {
    A_CORE_ERROR("CookedMaterialAsset: could not open '{}' for writing",
                 Path.string());
    return false;
  }

  const FileHeader Header{
      .Magic = {kMagic[0], kMagic[1], kMagic[2], kMagic[3]},
      .Version = kCookedMaterialFormatVersion,
      .AssetIdValue = Asset.Value,
      .BaseColorFactor = {Material.BaseColorFactor.r, Material.BaseColorFactor.g,
                          Material.BaseColorFactor.b, Material.BaseColorFactor.a},
      .Metallic = Material.Metallic,
      .Roughness = Material.Roughness,
      .TexturePathLength = static_cast<uint32_t>(Material.TextureAssetPath.size()),
  };
  if (!WriteValue(Stream, Header)) {
    return false;
  }

  if (!Material.TextureAssetPath.empty()) {
    Stream.write(Material.TextureAssetPath.data(),
                 static_cast<std::streamsize>(Material.TextureAssetPath.size()));
  }

  return Stream.good();
}

std::optional<CookedMaterialData>
LoadCookedMaterialAsset(const std::filesystem::path &Path) {
  std::ifstream Stream(Path, std::ios::binary);
  if (!Stream.is_open()) {
    return std::nullopt;
  }

  FileHeader Header{};
  if (!ReadValue(Stream, Header)) {
    A_CORE_WARN("CookedMaterialAsset: failed to read header from '{}'",
                Path.string());
    return std::nullopt;
  }

  if (Header.Magic[0] != kMagic[0] || Header.Magic[1] != kMagic[1] ||
      Header.Magic[2] != kMagic[2] || Header.Magic[3] != kMagic[3]) {
    A_CORE_WARN("CookedMaterialAsset: invalid magic in '{}'", Path.string());
    return std::nullopt;
  }

  if (Header.Version != kCookedMaterialFormatVersion) {
    A_CORE_WARN("CookedMaterialAsset: unsupported version {} in '{}'",
                Header.Version, Path.string());
    return std::nullopt;
  }

  CookedMaterialData Material{
      .BaseColorFactor = glm::vec4(Header.BaseColorFactor[0],
                                   Header.BaseColorFactor[1],
                                   Header.BaseColorFactor[2],
                                   Header.BaseColorFactor[3]),
      .Metallic = Header.Metallic,
      .Roughness = Header.Roughness,
  };

  if (Header.TexturePathLength > 0) {
    Material.TextureAssetPath.resize(Header.TexturePathLength);
    Stream.read(Material.TextureAssetPath.data(),
                static_cast<std::streamsize>(Material.TextureAssetPath.size()));
    if (!Stream.good()) {
      return std::nullopt;
    }
  }

  return Material;
}

} // namespace Axiom::Assets
