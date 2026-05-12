#pragma once

#include "Session/SessionTypes.h"
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Axiom::Assets {

enum class AssetKind { Mesh, Texture, Material, Unknown };

struct AssetDescriptor {
  AssetId Id;
  std::string Name;
  AssetKind Kind;
  std::string RelativePath;
};

class IAssetSource {
public:
  virtual ~IAssetSource() = default;
  virtual std::optional<std::filesystem::path> Resolve(AssetId Id) const = 0;
  virtual std::vector<AssetDescriptor> List() const = 0;
};

AssetId AssetIdFromRelativePath(const std::filesystem::path &RelPath);

// Scans a root directory on disk and maps discovered content files to stable
// AssetIds derived from their relative paths.
class LocalAssetSource : public IAssetSource {
public:
  explicit LocalAssetSource(std::filesystem::path Root);

  std::optional<std::filesystem::path> Resolve(AssetId Id) const override;
  std::vector<AssetDescriptor> List() const override;

  // Direct path join — used by shader and other engine-internal lookups.
  std::filesystem::path ResolveRelative(std::string_view RelPath) const;

private:
  std::filesystem::path m_Root;
  std::vector<AssetDescriptor> m_Index;
};

// Resolves cooked asset binaries from the cook manifest inside Content/Cooked.
class CookedAssetSource : public IAssetSource {
public:
  explicit CookedAssetSource(std::filesystem::path ContentRoot);

  std::optional<std::filesystem::path> Resolve(AssetId Id) const override;
  std::vector<AssetDescriptor> List() const override;
  bool HasManifest() const;

private:
  std::filesystem::path m_ContentRoot;
  std::vector<AssetDescriptor> m_Index;
};

} // namespace Axiom::Assets
