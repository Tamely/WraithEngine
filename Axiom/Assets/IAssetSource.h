#pragma once

#include "Session/SessionTypes.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Axiom::Assets {

enum class AssetKind { Mesh, Texture };

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

} // namespace Axiom::Assets
