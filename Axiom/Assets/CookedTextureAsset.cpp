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

bool WriteCommonHeader(std::ofstream &Stream, uint32_t Width, uint32_t Height,
                       uint32_t PixelByteCount, AssetId Asset) {
  const FileHeader Header{
      .Magic = {kMagic[0], kMagic[1], kMagic[2], kMagic[3]},
      .Version = kCookedTextureFormatVersion,
      .AssetIdValue = Asset.Value,
      .Width = Width,
      .Height = Height,
      .ChannelCount = 4,
      .PixelByteCount = PixelByteCount,
  };
  return WriteValue(Stream, Header);
}

bool ReadAndValidateHeader(std::ifstream &Stream, FileHeader &Header,
                           const std::filesystem::path &Path) {
  if (!ReadValue(Stream, Header)) {
    A_CORE_WARN("CookedTextureAsset: failed to read header from '{}'",
                Path.string());
    return false;
  }

  if (Header.Magic[0] != kMagic[0] || Header.Magic[1] != kMagic[1] ||
      Header.Magic[2] != kMagic[2] || Header.Magic[3] != kMagic[3]) {
    A_CORE_WARN("CookedTextureAsset: invalid magic in '{}'", Path.string());
    return false;
  }

  if (Header.Version != 1 && Header.Version != kCookedTextureFormatVersion) {
    A_CORE_WARN("CookedTextureAsset: unsupported version {} in '{}'",
                Header.Version, Path.string());
    return false;
  }

  if (Header.ChannelCount != 4) {
    A_CORE_WARN("CookedTextureAsset: unsupported channel count {} in '{}'",
                Header.ChannelCount, Path.string());
    return false;
  }

  return true;
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

  if (!WriteCommonHeader(Stream, Texture.Width, Texture.Height,
                         static_cast<uint32_t>(Texture.Pixels.size()), Asset))
    return false;

  const auto Format =
      static_cast<uint32_t>(CookedTexturePixelFormat::RGBA8);
  if (!WriteValue(Stream, Format))
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
  if (!ReadAndValidateHeader(Stream, Header, Path)) {
    return std::nullopt;
  }

  CookedTexturePixelFormat PixelFormat = CookedTexturePixelFormat::RGBA8;
  if (Header.Version >= 2) {
    uint32_t FormatRaw = 0;
    if (!ReadValue(Stream, FormatRaw)) {
      A_CORE_WARN("CookedTextureAsset: failed to read pixel format from '{}'",
                  Path.string());
      return std::nullopt;
    }
    PixelFormat = static_cast<CookedTexturePixelFormat>(FormatRaw);
  }

  if (PixelFormat != CookedTexturePixelFormat::RGBA8) {
    A_CORE_WARN("CookedTextureAsset: '{}' is an HDR texture; use "
                "LoadCookedHDRTextureAsset instead",
                Path.string());
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

bool SaveCookedHDRTextureAsset(const std::filesystem::path &Path,
                               const HDRTextureSourceData &Texture,
                               AssetId Asset) {
  if (!Texture.IsValid()) {
    A_CORE_WARN("CookedTextureAsset: refusing to save invalid HDR texture '{}'",
                Path.string());
    return false;
  }

  std::ofstream Stream(Path, std::ios::binary);
  if (!Stream.is_open()) {
    A_CORE_ERROR("CookedTextureAsset: could not open '{}' for writing",
                 Path.string());
    return false;
  }

  const uint32_t PixelByteCount =
      static_cast<uint32_t>(Texture.Pixels.size() * sizeof(float));
  if (!WriteCommonHeader(Stream, Texture.Width, Texture.Height, PixelByteCount,
                         Asset))
    return false;

  const auto Format =
      static_cast<uint32_t>(CookedTexturePixelFormat::RGBA32F);
  if (!WriteValue(Stream, Format))
    return false;

  Stream.write(reinterpret_cast<const char *>(Texture.Pixels.data()),
               static_cast<std::streamsize>(PixelByteCount));
  return Stream.good();
}

std::optional<HDRTextureSourceData>
LoadCookedHDRTextureAsset(const std::filesystem::path &Path) {
  std::ifstream Stream(Path, std::ios::binary);
  if (!Stream.is_open()) {
    return std::nullopt;
  }

  FileHeader Header{};
  if (!ReadAndValidateHeader(Stream, Header, Path)) {
    return std::nullopt;
  }

  if (Header.Version < 2) {
    A_CORE_WARN("CookedTextureAsset: '{}' is a v1 LDR texture, not HDR",
                Path.string());
    return std::nullopt;
  }

  uint32_t FormatRaw = 0;
  if (!ReadValue(Stream, FormatRaw)) {
    A_CORE_WARN("CookedTextureAsset: failed to read pixel format from '{}'",
                Path.string());
    return std::nullopt;
  }

  if (static_cast<CookedTexturePixelFormat>(FormatRaw) !=
      CookedTexturePixelFormat::RGBA32F) {
    A_CORE_WARN("CookedTextureAsset: '{}' is not an HDR texture", Path.string());
    return std::nullopt;
  }

  HDRTextureSourceData Texture;
  Texture.Width = Header.Width;
  Texture.Height = Header.Height;
  const size_t FloatCount = Header.PixelByteCount / sizeof(float);
  Texture.Pixels.resize(FloatCount);
  Stream.read(reinterpret_cast<char *>(Texture.Pixels.data()),
              static_cast<std::streamsize>(Header.PixelByteCount));
  if (!Stream.good()) {
    return std::nullopt;
  }

  if (!Texture.IsValid()) {
    A_CORE_WARN("CookedTextureAsset: decoded invalid HDR payload from '{}'",
                Path.string());
    return std::nullopt;
  }

  return Texture;
}

} // namespace Axiom::Assets
