#include "CookedTextureAsset.h"

#include "Core/Log.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <type_traits>

namespace Axiom::Assets {
namespace {

constexpr std::array<char, 4> kMagic = {'W', 'T', 'E', 'X'};

struct FileHeader {
  char Magic[4];
  uint32_t Version;
  uint64_t AssetIdValue;
  uint32_t Width;
  uint32_t Height;
  uint32_t ChannelCount;
  uint32_t PixelByteCount;
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

bool SaveCookedTextureAsset(const std::filesystem::path &Path,
                            const TextureSourceData &Texture, AssetId Asset) {
  if (!Texture.IsValid()) {
    A_CORE_WARN("CookedTextureAsset: refusing to save invalid texture '{}'",
                Path.string());
    return false;
  }

  std::ofstream Stream(Path, std::ios::binary);
  if (!Stream.is_open()) {
    A_CORE_ERROR("CookedTextureAsset: could not open '{}' for writing",
                 Path.string());
    return false;
  }

  const FileHeader Header{
      .Magic = {kMagic[0], kMagic[1], kMagic[2], kMagic[3]},
      .Version = kCookedTextureFormatVersion,
      .AssetIdValue = Asset.Value,
      .Width = Texture.Width,
      .Height = Texture.Height,
      .ChannelCount = 4,
      .PixelByteCount = static_cast<uint32_t>(Texture.Pixels.size()),
  };
  if (!WriteValue(Stream, Header))
    return false;

  Stream.write(reinterpret_cast<const char *>(Texture.Pixels.data()),
               static_cast<std::streamsize>(Texture.Pixels.size()));
  return Stream.good();
}

std::optional<TextureSourceData>
LoadCookedTextureAsset(const std::filesystem::path &Path) {
  std::ifstream Stream(Path, std::ios::binary);
  if (!Stream.is_open()) {
    return std::nullopt;
  }

  FileHeader Header{};
  if (!ReadValue(Stream, Header)) {
    A_CORE_WARN("CookedTextureAsset: failed to read header from '{}'",
                Path.string());
    return std::nullopt;
  }

  if (Header.Magic[0] != kMagic[0] || Header.Magic[1] != kMagic[1] ||
      Header.Magic[2] != kMagic[2] || Header.Magic[3] != kMagic[3]) {
    A_CORE_WARN("CookedTextureAsset: invalid magic in '{}'", Path.string());
    return std::nullopt;
  }

  if (Header.Version != kCookedTextureFormatVersion) {
    A_CORE_WARN("CookedTextureAsset: unsupported version {} in '{}'",
                Header.Version, Path.string());
    return std::nullopt;
  }

  if (Header.ChannelCount != 4) {
    A_CORE_WARN("CookedTextureAsset: unsupported channel count {} in '{}'",
                Header.ChannelCount, Path.string());
    return std::nullopt;
  }

  TextureSourceData Texture;
  Texture.Width = Header.Width;
  Texture.Height = Header.Height;
  Texture.Pixels.resize(Header.PixelByteCount);
  Stream.read(reinterpret_cast<char *>(Texture.Pixels.data()),
              static_cast<std::streamsize>(Texture.Pixels.size()));
  if (!Stream.good()) {
    return std::nullopt;
  }

  if (!Texture.IsValid()) {
    A_CORE_WARN("CookedTextureAsset: decoded invalid texture payload from '{}'",
                Path.string());
    return std::nullopt;
  }

  return Texture;
}

} // namespace Axiom::Assets
